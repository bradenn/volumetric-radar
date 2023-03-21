package pkg

import (
	"bridge/internal/infrastructure/log"
	"encoding/json"
	"fmt"
	"github.com/gorilla/websocket"
	"gonum.org/v1/gonum/dsp/fourier"
	"math"
	"math/cmplx"
	"sync"
	"time"
)

type Metadata struct {
	Name      string  `json:"name"`
	Mac       string  `json:"mac"`
	Connected bool    `json:"connected"`
	Base      int     `json:"base"`
	Samples   int     `json:"samples"`
	Window    int     `json:"window"`
	Frequency float64 `json:"frequency"`
	Prf       int     `json:"prf"`
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
		Prf       int     `json:"prf"`
		Window    int     `json:"window"`
		Bits      int     `json:"bits"`
		Pulse     int     `json:"pulse"`
	} `json:"adc"`
}

const (
	FrameDigest = 1000
	FrameRender = 16777 * 2
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
	Rate float64 `json:"rate"`
}

type Unit struct {
	Channels []Channel `json:"channels"`
	Metadata Metadata  `json:"metadata"`
	Phase    []float64 `json:"phase"`
	Distance []float64 `json:"distance"`
	Rate     float64   `json:"rate"`
}

const PRF = 1000
const Pulse = 500
const SampleRate = 83333 / 4

const BufferSize = 16
const IncomingSize = 256
const Frame = BufferSize * IncomingSize

func calculateRange(freq float64) float64 {
	frequency := freq
	sweepTime := 250e-6
	c := 2.99e8
	sweepBandwidth := 100e6

	// calculate the range using the voltage difference and the speed of light
	rangeMeters := (frequency * sweepTime * c) / (2 * sweepBandwidth)

	return rangeMeters
}

type RemoteUnit struct {
	Unit Unit

	mutex   sync.RWMutex
	display sync.RWMutex

	buffer   map[int][]complex128
	velocity map[int][]float64

	dsp map[int][]float64

	index int

	connection *websocket.Conn
	outbound   chan []byte
	done       chan bool

	total int
	start int64
	rate  float64

	address string
}

func stretchProcessing(signal []complex128) []complex128 {
	// Calculate the FFT of the signal
	cFFT := fourier.NewCmplxFFT(len(signal))
	sFFT := cFFT.Coefficients(nil, signal)

	// Define a chirp signal with a linear frequency sweep
	chirp := make([]complex128, len(signal))
	for i := range chirp {
		t := float64(i) / float64(len(chirp))
		chirp[i] = complex(real(sFFT[i])*math.Cos(2*math.Pi*t), -imag(sFFT[i])*math.Sin(2*math.Pi*t))
	}

	// Calculate the IFFT of the chirp signal
	stretched := cFFT.Sequence(nil, chirp)

	return stretched
}

func fmcwFFT(fft []complex128) {
	n := len(fft)
	half := n / 2
	for i := 0; i < half; i++ {
		fft[i], fft[i+half] = fft[i+half], fft[i]
	}
}

func downsample(input []float64, n int) []float64 {
	if len(input) <= n {
		// nothing to downsample, return the input as is
		return input
	}

	output := make([]float64, n)

	// compute the downsample factor
	factor := float64(len(input)) / float64(n)

	for i := 0; i < n; i++ {
		// compute the start and end indices of the input signal to be averaged
		start := int(math.Floor(float64(i) * factor))
		end := int(math.Floor(float64(i+1) * factor))

		// compute the average of the input signal over the current range
		sum := 0.0
		count := 0
		for j := start; j < end; j++ {
			sum += input[j]
			count++
		}
		output[i] = sum / float64(count)
	}

	return output
}

var minHz = 10000.0
var maxHz = -10000.0

func (r *RemoteUnit) digest() {
	r.index = (r.index + 1) % BufferSize
	r.mutex.Lock()
	if len(r.buffer[0]) < BufferSize*IncomingSize || len(r.buffer[1]) < BufferSize*IncomingSize {
		r.mutex.Unlock()
		return
	}

	//fmt.Printf("Min: %.4f, Max: %.4f\n", minHz, maxHz)

	cmp1 := r.buffer[0]
	h1 := cmp1
	cmp2 := r.buffer[1]
	h2 := cmp2
	r.mutex.Unlock()

	unit := Unit{
		Channels: make([]Channel, 2),
		Metadata: r.Unit.Metadata,
	}

	unit.Channels[0].SignalI = []float64{}
	unit.Channels[0].SignalQ = []float64{}
	unit.Channels[1].SignalI = []float64{}
	unit.Channels[1].SignalQ = []float64{}
	unit.Phase = []float64{}
	unit.Distance = []float64{}
	//for i := range cmp1 {
	//	fd := cmplx.Phase(cmp1[i]) - cmplx.Phase(cmp2[i])
	//	fs := math.Asin(fd * 0.2257)
	//	if fs != math.NaN() {
	//		unit.Phase = append(unit.Phase, fs)
	//	}
	//}

	//data := analyze(cmp1, cmp2)

	//filter

	cFFT := fourier.NewCmplxFFT(len(cmp1))
	cmp1 = cFFT.Coefficients(nil, cmp1)
	cmp2 = cFFT.Coefficients(nil, cmp2)

	//fmcwFFT(cmp1)
	//fmcwFFT(cmp2)
	//
	//cmp2 = stretchProcessing(cmp2)
	//cmp1 = stretchProcessing(cmp1)

	unit.Channels[0].Spectrum = []float64{}
	for i := range cmp1 {
		val := cmplx.Abs(cmp1[i])
		unit.Channels[0].Spectrum = append(unit.Channels[0].Spectrum, val)
	}

	unit.Channels[1].Spectrum = []float64{}
	for i := range cmp2 {
		val := cmplx.Abs(cmp2[i])
		unit.Channels[1].Spectrum = append(unit.Channels[1].Spectrum, val)
	}

	mx1 := -1.0
	for _, f := range unit.Channels[0].Spectrum {
		if f > mx1 {
			mx1 = f
		}
	}

	mx2 := -1.0
	for _, f := range unit.Channels[1].Spectrum {
		if f > mx2 {
			mx2 = f
		}
	}

	pk1 := findLocalMaxima(unit.Channels[0].Spectrum, mx1/2)
	pk2 := findLocalMaxima(unit.Channels[1].Spectrum, mx2/2)

	unit.Channels[0].Peaks = []float64{}
	for i := range pk1 {
		unit.Channels[0].Peaks = append(unit.Channels[0].Peaks, float64(pk1[i]))
	}
	unit.Channels[1].Peaks = []float64{}
	for i := range pk2 {
		unit.Channels[1].Peaks = append(unit.Channels[1].Peaks, float64(pk2[i]))
	}

	var freq1 []float64
	for i := range unit.Channels[0].Spectrum {
		freq1 = append(freq1, (20833.25*float64(i))/float64(len(cmp1)))

	}

	var freq2 []float64
	for i := range unit.Channels[1].Spectrum {
		freq2 = append(freq2, (20833.25*float64(i))/float64(len(cmp2)))

	}
	unit.Channels[0].Spectrum = downsample(unit.Channels[0].Spectrum, 1024)
	unit.Channels[1].Spectrum = downsample(unit.Channels[1].Spectrum, 1024)
	//unit.Channels[0].Spectrum = []float64{}
	//
	//unit.Channels[0].Frequencies = freq1
	//unit.Channels[1].Frequencies = freq2

	unit.Channels[0].SignalI = []float64{}
	unit.Channels[0].SignalQ = []float64{}
	h1 = cFFT.Sequence(nil, cmp1)
	for i := range h1 {
		unit.Channels[0].SignalI = append(unit.Channels[0].SignalI, real(h1[i]))
		unit.Channels[0].SignalQ = append(unit.Channels[0].SignalQ, imag(h1[i]))
	}

	unit.Channels[1].SignalI = []float64{}
	unit.Channels[1].SignalQ = []float64{}
	h2 = cFFT.Sequence(nil, cmp2)
	for i := range h2 {
		unit.Channels[1].SignalI = append(unit.Channels[1].SignalI, real(h2[i]))
		unit.Channels[1].SignalQ = append(unit.Channels[1].SignalQ, imag(h2[i]))
	}

	unit.Metadata.Window = BufferSize * IncomingSize
	unit.Rate = r.rate

	unit.Channels[0].SignalI = downsample(unit.Channels[0].SignalI, 2048)
	unit.Channels[0].SignalQ = downsample(unit.Channels[0].SignalQ, 2048)
	unit.Channels[1].SignalI = downsample(unit.Channels[1].SignalI, 2048)
	unit.Channels[1].SignalQ = downsample(unit.Channels[1].SignalQ, 2048)
	//avg := 0.0
	//cnt := 0.0
	unit.Distance = []float64{}
	unit.Phase = []float64{}
	//last := 0.0
	//for _, i2 := range pk1 {
	//	//unit.Channels[0].Spectrum = append(unit.Channels[0].Spectrum, calculateRange(freq1[i2]))
	//	ra := freq1[i2]
	//	if last != 0 {
	//		unit.Distance = append(unit.Distance, calculateRange(ra-last))
	//		unit.Phase = append(unit.Phase, math.Pi/2)
	//	} else {
	//		last = ra
	//	}
	//
	//}

	//for _, i2 := range pk2 {
	//	//unit.Channels[0].Spectrum = append(unit.Channels[0].Spectrum, calculateRange(freq1[i2]))
	//	ra := calculateRange(freq2[i2])
	//	unit.Distance = append(unit.Distance, ra)
	//	unit.Phase = append(unit.Phase, math.Pi/2)
	//}
	//r.dsp[5] = append(r.dsp[5], avg/cnt)

	r.mutex.Lock()
	unit.Phase = r.dsp[4]
	//var ph []float64
	//for _, f := range r.dsp[4] {
	//	value := f / (200.0 / 1000.0 / 1000.0)
	//	ph = append(ph, value*8192)
	//}
	le := 128
	if len(r.dsp[5]) >= le {
		r.dsp[5] = r.dsp[5][len(r.dsp[5])-le:]
	}

	//r.buffer[0] = []complex128{}
	//r.buffer[1] = []complex128{}
	r.mutex.Unlock()

	r.display.Lock()
	r.Unit = unit
	r.display.Unlock()

}

func (r *RemoteUnit) tick() {
	r.display.RLock()
	unit := r.Unit
	r.display.RUnlock()

	marshal, err := json.Marshal(unit)
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
		return
	}

	var incoming [][]float64

	for _, result := range packet.Results {
		incoming = append(incoming, hexStringToIntArray(result))
	}
	r.total += len(incoming[0])
	//if time.Since(r.start) > time.Microsecond*FrameRender {
	//
	//	//r.start = time.Now()
	//
	//}
	if r.total > 0 {
		r.rate = float64(r.total) / (float64(packet.Time-r.start) / (1000.0 * 1000.0))
		//fmt.Println(float64(packet.Time-r.start) / (1000.0 * 1000.0))
		r.total = 0
		r.start = packet.Time
	}

	var cmp1 []complex128
	var cmp2 []complex128
	for i := range incoming[0] {
		cmp1 = append(cmp1, complex(incoming[0][i], incoming[1][i]))
		cmp2 = append(cmp2, complex(incoming[3][i], incoming[2][i]))
	}

	for _, f := range incoming[4] {
		if minHz > f {
			minHz = f
		}
		if maxHz < f {
			maxHz = f
		}
	}
	r.mutex.Lock()
	// 9600 s/s 768.04915515 -> 12.4992
	r.buffer[0] = append(r.buffer[0], cmp1...)
	r.buffer[1] = append(r.buffer[1], cmp2...)

	if len(r.buffer[0]) >= IncomingSize*BufferSize {
		r.buffer[0] = r.buffer[0][len(r.buffer[0])-(IncomingSize*BufferSize):]
	}
	if len(r.buffer[1]) >= IncomingSize*BufferSize {
		r.buffer[1] = r.buffer[1][len(r.buffer[1])-(IncomingSize*BufferSize):]
	}
	//r.dsp[0] = append(r.dsp[0], incoming[0]...)
	//r.dsp[1] = append(r.dsp[1], incoming[1]...)
	//r.dsp[2] = append(r.dsp[2], incoming[2]...)
	//r.dsp[3] = append(r.dsp[3], incoming[3]...)
	r.dsp[4] = incoming[4]

	r.mutex.Unlock()

}

func (r *RemoteUnit) connect() error {
	r.total = 0
	// Initialize local variables
	var err error
	var conn *websocket.Conn
	// Connect to the address
	conn, _, err = websocket.DefaultDialer.Dial(r.address, nil)
	if err != nil {
		return err
	}
	// Set the close handler to send a bool to the done channel
	conn.SetCloseHandler(func(code int, text string) error {
		r.done <- true

		return nil
	})
	// Send an initial ping message to being receiving
	err = conn.WriteMessage(websocket.TextMessage, []byte("Ping!"))
	if err != nil {
		return err
	}
	// Read the metadata response
	_, data, err := conn.ReadMessage()
	if err != nil {
		return err
	}
	// Parse the metadata
	unit := UnitData{}
	err = json.Unmarshal(data, &unit)
	if err != nil {
		return err
	}
	// Metadata converting
	r.display.Lock()
	r.Unit.Metadata = Metadata{
		Base:      unit.Base,
		Samples:   unit.Adc.Samples,
		Frequency: unit.Adc.Frequency,
		Window:    unit.Adc.Window,
		Prf:       unit.Adc.Prf,
		Mac:       unit.Mac,
		Name:      unit.Name,
		XFov:      unit.XFov,
		YFov:      unit.YFov,
		Chirp:     unit.Adc.Pulse,
	}
	r.Unit.Metadata.Connected = true
	r.display.Unlock()
	r.connection = conn

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
	for {
		if r.connection == nil {
			return
		}
		err := r.connection.SetReadDeadline(time.Now().Add(time.Millisecond * 500))
		if err != nil {
			return
		}
		_, data, err := r.connection.ReadMessage()
		if err != nil {
			r.connection.Close()
			return
		}
		r.process(data)
	}
	//scanner := bufio.NewScanner(r.connection)
	//scanner.Split(scanNullTerminatedJSON)
	//
	//for scanner.Scan() {
	//	r.process(scanner.Bytes())
	//}
	//if scanner.Err() != nil {
	//	fmt.Println("Scanner error:", scanner.Err())
	//}
}

func (r *RemoteUnit) watchdog() {
	for {
		select {
		case <-r.done:
			r.display.Lock()
			log.Event("Unit '%s' @ %s disconnected. Reconnecting...", r.Unit.Metadata.Name, r.address)
			r.Unit.Metadata.Connected = false
			r.display.Unlock()
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
		r.display.RLock()
		log.Log("Unit '%s' @ %s connection failed", r.Unit.Metadata.Name, r.address)
		r.display.RUnlock()
		return
	}
}

func NewRemoteUnit(addr string, out chan []byte) (*RemoteUnit, error) {
	r := RemoteUnit{
		mutex:   sync.RWMutex{},
		display: sync.RWMutex{},
		Unit: Unit{
			Phase:    []float64{},
			Distance: []float64{},
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
