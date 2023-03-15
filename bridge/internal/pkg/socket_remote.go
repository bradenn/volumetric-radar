package pkg

import (
	"bridge/internal/infrastructure/log"
	"bufio"
	"encoding/json"
	"fmt"
	fft2 "github.com/mjibson/go-dsp/fft"
	"gonum.org/v1/gonum/dsp/fourier"
	"gonum.org/v1/gonum/dsp/window"
	"math"
	"math/cmplx"
	"net"
	"os"
	"sync"
	"time"
)

type Metadata struct {
	Name      string  `json:"name"`
	Mac       string  `json:"mac"`
	Base      int     `json:"base"`
	Samples   int     `json:"samples"`
	Window    int     `json:"window"`
	Frequency float64 `json:"frequency"`
	Chirp     int     `json:"chirp"`
	XFov      int     `json:"xFov"`
	YFov      int     `json:"yFov"`
}
type UnitData struct {
	Name string `json:"name"`
	Mac  string `json:"mac"`
	Base int    `json:"base"`
	XFov int    `json:"xFov"`
	YFov int    `json:"yFov"`
	Adc  struct {
		Base      float64 `json:"base"`
		Frequency float64 `json:"frequency"`
		Samples   int     `json:"samples"`
		Window    int     `json:"window"`
		Bits      int     `json:"bits"`
		Pulse     int     `json:"pulse"`
	} `json:"adc"`
}

const (
	FrameDigest = 33333 / 2
	FrameRender = 33333
)

type Diagnostic struct {
	Size struct {
		Inbound  int `json:"inbound"`
		Outbound int `json:"outbound"`
	} `json:"size"`
	Temporal struct {
		Absolute int `json:"inbound"`
		Slice    int `json:"outbound"`
	} `json:"temporal"`
}

type Unit struct {
	Channels []Channel `json:"channels"`
	Metadata Metadata  `json:"metadata"`
	Phase    []float64 `json:"phase"`
	Distance []float64 `json:"distance"`
}

const BufferSize = 8
const IncomingSize = 128

type RemoteUnit struct {
	Unit Unit

	mutex sync.RWMutex

	buffer   map[int][]complex128
	velocity map[int][]float64

	dsp map[int][]float64

	index int

	connection *net.TCPConn
	outbound   chan []byte
	done       chan bool

	total int
	start time.Time
	rate  float64

	address string
}

// Define the carrier frequency of the radar system
const fc = 24.125e9

func dopplerFFT(signal1, signal2 []complex128, sampleRate float64) []float64 {
	// Calculate the time interval between the two received signals
	deltaT := 1 / sampleRate

	// Calculate the cross-correlation between the two received signals
	correlation := crossCorrelation(signal1, signal2)

	// Apply the FFT to the cross-correlation
	ft := fft(correlation)

	// Calculate the phase difference between the two received signals
	phaseDifference := make([]float64, len(ft))
	for i, val := range ft {
		phaseDifference[i] = cmplx.Phase(val)
	}

	// Calculate the Doppler shift
	dopplerShift := make([]float64, len(phaseDifference))
	for i, val := range phaseDifference {
		dopplerShift[i] = (val / (2 * math.Pi * deltaT)) * C / (2 * fc)
	}

	return dopplerShift
}

// Cross-correlation function
func crossCorrelation(signal1, signal2 []complex128) []complex128 {
	// Pad the signals to the same length
	length := len(signal1) + len(signal2) - 1
	signal1 = append(signal1, make([]complex128, length-len(signal1))...)
	signal2 = append(signal2, make([]complex128, length-len(signal2))...)

	// Apply the FFT to both signals
	fft1 := fft(signal1)
	fft22 := fft(signal2)

	// Multiply the FFT of the two signals element-wise
	mult := make([]complex128, length)
	for i := 0; i < length; i++ {
		mult[i] = fft1[i] * cmplx.Conj(fft22[i])
	}

	// Apply the inverse FFT to the result
	return ifft(mult)
}
func rangeFFT(signal [][]complex128, sampleRate float64) []float64 {
	// Calculate the time interval between samples
	deltaT := 1 / sampleRate

	// Apply the FFT to each range bin
	fftSignal := make([]float64, len(signal[0]))
	for i := 0; i < len(signal[0]); i++ {
		// Apply a window function to the range bin
		windowed := make([]complex128, len(signal))
		for j := 0; j < len(signal); j++ {
			windowed[j] = signal[j][i] * hammingWindow(len(signal[j]))
		}

		// Apply the FFT to the windowed range bin
		fftB := fft2.FFT(windowed)

		// Calculate the magnitude of the FFT and normalize it
		magnitude := make([]float64, len(fftB))
		for j, val := range fftB {
			magnitude[j] = cmplx.Abs(val) / float64(len(fftB))
		}

		// Calculate the range of the target using the FFT result
		rangeVal := (float64(i) / float64(len(signal[0]))) * (1 / (2 * deltaT * fc))
		fftSignal[i] = magnitude[0] * math.Cos(2*math.Pi*fc*rangeVal)
	}

	return fftSignal
}

func hammingWindow(length int) complex128 {
	alpha := 0.54
	beta := 0.46
	return complex(alpha-beta*math.Cos(2*math.Pi/float64(length-1)), 0)
}

// FFT function
func fft(signal []complex128) []complex128 {
	// Use the FFT function from the standard library
	return fft2.FFT(signal)
}

// Inverse FFT function
func ifft(signal []complex128) []complex128 {
	// Use the inverse FFT function from the standard library
	return fft2.IFFT(signal)
}
func (r *RemoteUnit) digest() {
	r.mutex.Lock()
	defer r.mutex.Unlock()

	r.Unit.Channels[0].SignalI = r.dsp[0]
	r.Unit.Channels[0].SignalQ = r.dsp[1]
	r.Unit.Channels[1].SignalI = r.dsp[2]
	r.Unit.Channels[1].SignalQ = r.dsp[3]

	r.index = (r.index + 1) % BufferSize
	if len(r.buffer[0]) < BufferSize*IncomingSize {
		return
	}

	cmp1 := r.buffer[0]
	cmp2 := r.buffer[1]

	fft := fourier.NewCmplxFFT(len(cmp1))

	cmp1 = fft.Coefficients(nil, cmp1)
	cmp2 = fft.Coefficients(nil, cmp2)
	////cmp1 = fourier.CoefficientsRadix2(cmp1)
	////cmp2 = fourier.CoefficientsRadix2(cmp2)
	//
	cmp1 = window.HammingComplex(cmp1)
	cmp2 = window.HammingComplex(cmp2)
	//

	r.Unit.Phase = []float64{}
	r.Unit.Distance = []float64{}
	for i := range cmp1 {
		deltaPhi := cmplx.Phase(cmp2[i]) - cmplx.Phase(cmp1[i])
		theta := math.Asin(0.2257 * deltaPhi)
		// Calculate the time interval between samples
		//dt := 1.0 / 24.125e9

		// Use the time interval and the angle estimate to estimate the distance to the object
		//distance := dt * C / (2.0 * math.Sin(theta))
		distance := 1.0

		// Do something with the distance estimate, such as printing it to the console
		if theta == math.NaN() {
			fmt.Println("sdds")
			theta = 0
		}

		r.Unit.Phase = append(r.Unit.Phase, theta)
		r.Unit.Distance = append(r.Unit.Distance, distance)
	}

	//correlation := crossCorrelatio	n(cmp1, cmp2)

	r.Unit.Channels[0].Spectrum = []float64{}
	for i := range cmp1 {
		val := math.Sqrt(real(cmp1[i])*real(cmp1[i]) + imag(cmp1[i])*imag(cmp1[i]))
		r.Unit.Channels[0].Spectrum = append(r.Unit.Channels[0].Spectrum, val)
	}

	r.Unit.Channels[1].Spectrum = []float64{}
	for i := range cmp2 {
		val := math.Sqrt(real(cmp2[i])*real(cmp2[i]) + imag(cmp2[i])*imag(cmp2[i]))
		r.Unit.Channels[1].Spectrum = append(r.Unit.Channels[1].Spectrum, val)
	}

	//mx := -1.0
	//for _, f := range r.Unit.Channels[0].Spectrum {
	//	if f > mx {
	//		mx = f
	//	}
	//}
	//
	//max := findLocalMaxima(r.Unit.Channels[0].Spectrum, mx/1.25)
	//var out []float64
	//for i := range max {
	//	out = append(out, float64(max[i]))
	//}

	//var freq []float64
	//for i, _ := range r.Unit.Channels[0].Spectrum {
	//	freq = append(freq, (r.Unit.Metadata.Frequency*float64(i))/float64(len(cmp1)))
	//}
	//
	//for _, ou := range out {
	//	r.Unit.Phase = append(r.Unit.Phase, freq[int(ou)])
	//}

	//r.Unit.Channels[0].Spectrum = r.velocity[0]
	r.Unit.Channels[0].Peaks = []float64{}
	r.Unit.Channels[0].Frequencies = []float64{}
	//
	//r.Unit.Channels[1].Spectrum = r.velocity[1]
	r.Unit.Channels[1].Peaks = []float64{}
	r.Unit.Channels[1].Frequencies = []float64{}
	//
	////
	//r.Unit.Channels[0].SignalI = []float64{}
	//r.Unit.Channels[0].SignalQ = []float64{}
	//cmp1i := fft.Sequence(nil, cmp1)
	//for i := range cmp1i {
	//	r.Unit.Channels[0].SignalI = append(r.Unit.Channels[0].SignalI, real(cmp1i[i]))
	//	r.Unit.Channels[0].SignalQ = append(r.Unit.Channels[0].SignalQ, imag(cmp1i[i]))
	//}
	//
	//r.Unit.Channels[1].SignalI = []float64{}
	//r.Unit.Channels[1].SignalQ = []float64{}
	//cmp2i := fft.Sequence(nil, cmp2)
	//for i := range cmp2i {
	//	r.Unit.Channels[1].SignalI = append(r.Unit.Channels[1].SignalI, real(cmp2i[i]))
	//	r.Unit.Channels[1].SignalQ = append(r.Unit.Channels[1].SignalQ, imag(cmp2i[i]))
	//}
	//
	//r.Unit.Metadata.Window = len(cmp1)

	//r.buffer[0] = []complex128{}
	//r.buffer[1] = []complex128{}

}

func (r *RemoteUnit) tick() {
	r.mutex.RLock()
	defer r.mutex.RUnlock()

	marshal, err := json.Marshal(r.Unit)
	if err != nil {
		log.Err(err)
		return
	}

	timer := time.NewTimer(time.Microsecond * FrameRender)
	select {
	case <-timer.C:
		log.Err(fmt.Errorf("outbound timeout"))
		return
	case r.outbound <- marshal:
		timer.Stop()
		return
	}
}

func (r *RemoteUnit) process(data []byte) {
	//fmt.Println(string(data))
	packet := Packet{}
	err := json.Unmarshal(data, &packet)
	if err != nil {
		log.Err(err)
		fmt.Println(string(data))
		return
	}

	var incoming [][]float64

	for _, result := range packet.Results {
		incoming = append(incoming, hexStringToIntArray(result))
	}

	r.total += len(incoming[0])
	if time.Since(r.start) > time.Second {
		r.rate = float64(r.total) / time.Since(r.start).Seconds()
		log.Event("Rate: %.4f", r.rate)
		r.start = time.Now()
		r.total = 0
	}

	r.dsp[0] = incoming[0]
	r.dsp[1] = incoming[1]
	r.dsp[2] = incoming[2]
	r.dsp[3] = incoming[3]

	var cmp1 []complex128
	var cmp2 []complex128
	for i := range incoming[0] {
		cmp1 = append(cmp1, complex(incoming[0][i], incoming[1][i]))
		cmp2 = append(cmp2, complex(incoming[3][i], incoming[2][i]))
	}

	r.mutex.Lock()
	defer r.mutex.Unlock()

	r.buffer[0] = append(cmp1, r.buffer[0]...)
	r.buffer[1] = append(cmp2, r.buffer[1]...)

	if len(r.buffer[0]) >= IncomingSize*BufferSize {
		r.buffer[0] = r.buffer[0][0 : IncomingSize*BufferSize]
		r.buffer[1] = r.buffer[1][0 : IncomingSize*BufferSize]
	}

}

func (r *RemoteUnit) connect() error {
	r.total = 0
	// Initialize local variables
	var err error
	addr, err := net.ResolveTCPAddr("tcp", "10.0.1.133:5545")
	if err != nil {
		fmt.Println(err)
		return err
	}

	tcp, err := net.DialTCP("tcp", nil, addr)
	if err != nil {
		fmt.Println(err)
		return err
	}

	// Send an initial ping message to being receiving
	_, err = tcp.Write([]byte("Hello"))
	if err != nil {
		fmt.Println(err)
		return err
	}
	reply := make([]byte, 2048)

	n, err := tcp.Read(reply)
	if err != nil {
		println("Write to server failed:", err.Error())
		os.Exit(1)
	}
	// Parse the metadata
	unit := UnitData{}
	err = json.Unmarshal(reply[0:n], &unit)
	if err != nil {
		return err
	}
	// Metadata converting
	r.Unit.Metadata = Metadata{
		Base:      unit.Base,
		Samples:   unit.Adc.Samples,
		Frequency: unit.Adc.Frequency,
		Window:    unit.Adc.Window,
		Mac:       unit.Mac,
		Name:      unit.Name,
		XFov:      unit.XFov,
		YFov:      unit.YFov,
		Chirp:     unit.Adc.Pulse,
	}

	fmt.Println(unit)
	r.connection = tcp

	log.Event("Unit '%s' @ %s connected", unit.Name, r.address)

	go func() {
		ticker := time.NewTicker(FrameRender * time.Microsecond)
		defer ticker.Stop()
		for {
			select {
			case _ = <-ticker.C:
				r.tick()
			}
		}
	}()

	go func() {
		ticker := time.NewTicker(FrameDigest * time.Microsecond)
		defer ticker.Stop()
		for {
			select {
			case _ = <-ticker.C:
				r.digest()
			}
		}
	}()

	return nil

}
func scanNullTerminatedJSON(data []byte, atEOF bool) (advance int, token []byte, err error) {
	if atEOF && len(data) == 0 {
		return 0, nil, nil
	}

	// find the null terminator
	i := 0
	for ; i < len(data); i++ {
		if data[i] == 0 {
			break
		}
	}

	if i == len(data) && !atEOF {
		// need more data
		return 0, nil, nil
	}

	return i + 1, data[:i], nil
}
func (r *RemoteUnit) listen() {

	scanner := bufio.NewScanner(r.connection)
	scanner.Split(scanNullTerminatedJSON)

	for scanner.Scan() {
		r.process(scanner.Bytes())
	}
	if scanner.Err() != nil {
		fmt.Println("Scanner error:", scanner.Err())
	}
}

func (r *RemoteUnit) watchdog() {
	for {
		select {
		case <-r.done:
			log.Event("Unit '%s' @ %s disconnected. Reconnecting...", r.Unit.Metadata.Name, r.address)
			r.reconnect()
		}
	}
}

func (r *RemoteUnit) reconnect() {
	if r.connection != nil {
		return
	}
	err := r.connect()
	if err != nil {
		log.Log("Unit '%s' @ %s connection failed", r.Unit.Metadata.Name, r.address)
		return
	}
}

func NewRemoteUnit(addr string, out chan []byte) (*RemoteUnit, error) {
	r := RemoteUnit{
		mutex: sync.RWMutex{},
		Unit: Unit{

			Channels: []Channel{{
				SignalI:     []float64{},
				SignalQ:     []float64{},
				Phase:       []float64{},
				Spectrum:    []float64{},
				Frequency:   0,
				Frequencies: []float64{},
				Peaks:       []float64{},
			}, {
				SignalI:     []float64{},
				SignalQ:     []float64{},
				Phase:       []float64{},
				Spectrum:    []float64{},
				Frequency:   0,
				Frequencies: []float64{},
				Peaks:       []float64{},
			}},
			Metadata: Metadata{
				Name:      "",
				Mac:       "",
				Base:      0,
				Samples:   0,
				Frequency: 0,
				Chirp:     0,
				XFov:      0,
				YFov:      0,
			},
		},
		done:     make(chan bool),
		outbound: out,
		index:    0,
		buffer:   map[int][]complex128{},
		velocity: map[int][]float64{},
		dsp:      map[int][]float64{},
		address:  addr,
	}
	r.buffer[0] = []complex128{}
	r.buffer[1] = []complex128{}
	r.velocity[0] = []float64{}
	r.velocity[1] = []float64{}
	r.velocity[2] = []float64{}
	r.velocity[3] = []float64{}
	r.reconnect()

	go r.watchdog()
	go r.listen()
	r.start = time.Now()
	return &r, nil
}

type UnitServer struct {
	connections map[string]*RemoteUnit
	outbound    chan []byte
}

func (s *UnitServer) AddUnit(addr string) {
	unit, err := NewRemoteUnit(addr, s.outbound)
	if err != nil {
		log.Err(err)
		return
	}
	mac := unit.Unit.Metadata.Mac
	if s.connections[mac] != nil {
		err = s.connections[mac].connection.Close()
		if err != nil {
		}
		s.connections[mac] = nil
	}
	s.connections[unit.Unit.Metadata.Mac] = unit
}

func NewUnitServer(out chan []byte) (*UnitServer, error) {
	s := UnitServer{
		connections: map[string]*RemoteUnit{},
		outbound:    out,
	}

	return &s, nil
}
