package pkg

import (
	"bridge/internal/infrastructure/log"
	"encoding/json"
	"fmt"
	"github.com/gorilla/websocket"
	"gonum.org/v1/gonum/dsp/fourier"
	"gonum.org/v1/gonum/dsp/window"
	"math"
	"math/cmplx"
	"strconv"
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

type Unit struct {
	Channels []Channel `json:"channels"`
	Metadata Metadata  `json:"metadata"`
	Phase    []float64 `json:"phase"`
	Distance []float64 `json:"distance"`
}

const BufferSize = 8
const IncomingSize = 512

type RemoteUnit struct {
	Unit Unit

	buffer   map[int][]complex128
	velocity map[int][]float64
	index    int

	connection *websocket.Conn
	outbound   chan []byte
	done       chan bool

	total int
	start time.Time

	address string
}

func CalculateRange(fft0, fft1 []complex128, centerFrequencyHz, chirpDurationSec, chirpBandwidthHz, speedOfLightMps, sweepSlopeHzps float64) float64 {
	// Find the maximum index of the FFT output
	maxIdx := 0
	maxMag := 0.0
	for i := 0; i < len(fft0); i++ {
		mag := cmplx.Abs(fft0[i] * fft1[i])
		if mag > maxMag {
			maxMag = mag
			maxIdx = i
		}
	}

	// Calculate the range using the maximum index
	rangeVal := speedOfLightMps * (centerFrequencyHz * chirpDurationSec / (2 * chirpBandwidthHz)) * sweepSlopeHzps * float64(maxIdx)
	return rangeVal
}
func BinToFreq(binIndex, fftSize int, sweepRate, chirpDuration float64) float64 {
	//c := 3e8 // speed of light in meters per second
	rampDuration := chirpDuration / 2.0
	sweepBandwidth := sweepRate * rampDuration
	freqResolution := sweepBandwidth / float64(fftSize)
	return float64(binIndex)*freqResolution - sweepBandwidth/2.0
}

const C = 299792458
const Fs = 13750.0

func unpackArray(hexString string) []float64 {
	packedValues := make([]float64, 0)
	for i := 0; i < len(hexString); i += 8 {
		packedValue, _ := strconv.ParseUint(hexString[i:i+8], 16, 32)
		value1 := float64(packedValue & 0xffff)
		value2 := float64((packedValue >> 16) & 0xffff)
		packedValues = append(packedValues, value2, value1)
	}

	return packedValues
}

func (r *RemoteUnit) process(data []byte) {

	packet := Packet{}
	err := json.Unmarshal(data, &packet)
	if err != nil {
		log.Err(err)

		return
	}

	r.Unit.Channels[0].SignalI = unpackArray(packet.Ch0)
	r.Unit.Channels[0].SignalQ = unpackArray(packet.Ch1)
	r.Unit.Channels[1].SignalI = unpackArray(packet.Ch3)
	r.Unit.Channels[1].SignalQ = unpackArray(packet.Ch2)

	r.total = r.total + (len(r.Unit.Channels[0].SignalI))
	fmt.Printf("%f\n", float64(r.total)/time.Since(r.start).Seconds())
	if time.Since(r.start).Seconds() > 5 {
		r.total = 0
		r.start = time.Now()
	}

	//if len(packet.Ch0) <= 0 {
	//	return
	//}
	//if len(packet.Ch1) <= 0 {
	//	return
	//}
	//if len(packet.Ch2) <= 0 {
	//	return
	//}
	//if len(packet.Ch3) <= 0 {
	//	return
	//}

	var cmp1 []complex128
	var cmp2 []complex128

	for i := range r.Unit.Channels[0].SignalI {
		cmp1 = append(cmp1, complex(r.Unit.Channels[0].SignalI[i], r.Unit.Channels[0].SignalQ[i]))
	}
	for i := range r.Unit.Channels[1].SignalI {
		cmp2 = append(cmp2, complex(r.Unit.Channels[1].SignalI[i], r.Unit.Channels[1].SignalQ[i]))
	}

	r.buffer[0] = append(r.buffer[0], cmp1...)
	r.buffer[1] = append(r.buffer[1], cmp2...)

	r.index = (r.index + 1) % BufferSize
	if len(r.buffer[0]) < BufferSize*IncomingSize {
		return
	}

	cmp1 = r.buffer[0]
	cmp2 = r.buffer[1]

	r.Unit.Phase = []float64{}
	r.Unit.Distance = []float64{}

	//// Calculate the complex conjugate of cmp2
	//conj := make([]complex128, len(cmp2))
	//for i, val := range cmp2 {
	//	conj[i] = cmplx.Conj(val)
	//}
	//
	//// Calculate the product of cmp1 and the complex conjugate of cmp2
	//product := make([]complex128, len(cmp1))
	//for i, val := range cmp1 {
	//	product[i] = val * conj[i]
	//}
	//
	//// Take the inverse FFT of the product to get the phase difference
	//
	//phaseDiff := fft2.IFFT(product)
	//
	//for i, _ := range phaseDiff[0 : len(phaseDiff)/2] {
	//	deltaPhi := cmplx.Phase(phaseDiff[i])
	//	theta := math.Asin(0.2257 * deltaPhi)
	//	// Calculate the time interval between samples
	//	//dt := 1.0 / Fs
	//
	//	// Use the time interval and the angle estimate to estimate the distance to the object
	//	//distance := dt * C / (2.0 * math.Sin(theta))
	//
	//	// Do something with the distance estimate, such as printing it to the console
	//	r.Unit.Distance = append(r.Unit.Distance, rand.NormFloat64())
	//	r.Unit.Phase = append(r.Unit.Phase, math.Min(math.Max(theta, -math.MaxFloat64), math.MaxFloat64))
	//}

	//var ftn [][]complex128
	//idx := -1
	//for i := 0; i < len(cmp1); i++ {
	//	if i%IncomingSize == 0 {
	//		idx++
	//		ftn = append(ftn, []complex128{})
	//	}
	//	ftn[idx] = append(ftn[idx], cmp1[i])
	//
	//}
	//
	//iou := fft2.FFT2(ftn)

	fft := fourier.NewCmplxFFT(len(cmp2))
	cmp1 = window.HammingComplex(cmp1)
	cmp2 = window.HammingComplex(cmp2)
	cmp1 = fft.Coefficients(nil, cmp1)
	cmp2 = fft.Coefficients(nil, cmp2)

	//cmp1 = phaseDiff

	//cmp1 = fourier.CoefficientsRadix4(cmp1)
	//cmp2 = fourier.CoefficientsRadix4(cmp2)

	//r.Unit.Channels[0].Spectrum = []float64{}
	//for i := range iou {
	//	for j := range iou[i] {
	//		r.Unit.Channels[0].Spectrum = append(r.Unit.Channels[0].Spectrum, math.Max(math.Sqrt(real(iou[i][j])*real(iou[i][j])+imag(iou[i][j])*imag(iou[i][j])), 0))
	//	}
	//}

	if len(r.velocity[0]) < len(cmp1)/2 {
		r.velocity[0] = make([]float64, len(cmp1)/2)
	}

	if len(r.velocity[1]) < len(cmp2)/2 {
		r.velocity[1] = make([]float64, len(cmp2)/2)
	}

	r.Unit.Channels[0].Spectrum = []float64{}
	for i := range cmp1[0 : len(cmp1)/2] {
		r.Unit.Channels[0].Spectrum = append(r.Unit.Channels[0].Spectrum, math.Max(math.Sqrt(real(cmp1[i])*real(cmp1[i])+imag(cmp1[i])*imag(cmp1[i])), 0))
	}
	for i, f := range r.Unit.Channels[0].Spectrum {
		r.velocity[0][i] += f
		r.velocity[0][i] /= 2
	}

	r.Unit.Channels[1].Spectrum = []float64{}
	for i := range cmp2[0 : len(cmp2)/2] {
		r.Unit.Channels[1].Spectrum = append(r.Unit.Channels[1].Spectrum, math.Max(math.Sqrt(real(cmp2[i])*real(cmp2[i])+imag(cmp2[i])*imag(cmp2[i])), 0))
	}

	for i, f := range r.Unit.Channels[1].Spectrum {
		r.velocity[1][i] += f
		r.velocity[1][i] /= 2
	}

	mx := -1.0
	for _, f := range r.Unit.Channels[0].Spectrum {
		if f > mx {
			mx = f
		}
	}

	max := findLocalMaxima(r.Unit.Channels[0].Spectrum, mx/2)
	var out []float64
	for i := range max {
		out = append(out, float64(max[i]))
	}

	var freq []float64
	for i, _ := range r.Unit.Channels[0].Spectrum {
		freq = append(freq, (r.Unit.Metadata.Frequency*float64(i))/float64(len(cmp1)))
	}

	r.Unit.Channels[0].Spectrum = r.velocity[0]
	r.Unit.Channels[0].Peaks = out
	r.Unit.Channels[0].Frequencies = freq

	r.Unit.Channels[1].Spectrum = r.velocity[1]
	r.Unit.Channels[1].Peaks = []float64{}
	r.Unit.Channels[1].Frequencies = []float64{}

	r.buffer[0] = r.buffer[0][IncomingSize : IncomingSize*BufferSize]
	r.buffer[1] = r.buffer[1][IncomingSize : IncomingSize*BufferSize]

	marshal, err := json.Marshal(r.Unit)
	if err != nil {
		log.Err(err)
		return
	}
	//
	//r.Unit.Channels[0].SignalI = []float64{}
	//r.Unit.Channels[0].SignalQ = []float64{}
	//cmp1i := fft.Sequence(nil, cmp1)
	//for i := range cmp1i {
	//	r.Unit.Channels[0].SignalI = append(r.Unit.Channels[0].SignalI, real(cmp1i[i]))
	//	r.Unit.Channels[0].SignalQ = append(r.Unit.Channels[0].SignalQ, imag(cmp1i[i]))
	//}
	//r.Unit.Channels[1].SignalI = []float64{}
	//r.Unit.Channels[1].SignalQ = []float64{}
	//cmp2i := fft.Sequence(nil, cmp2)
	//for i := range cmp2i {
	//	r.Unit.Channels[1].SignalI = append(r.Unit.Channels[1].SignalI, real(cmp2i[i]))
	//	r.Unit.Channels[1].SignalQ = append(r.Unit.Channels[1].SignalQ, imag(cmp2i[i]))
	//}
	//
	//r.Unit.Metadata.Window = len(cmp1i)
	//fmt.Println(time.Since(st))
	timer := time.NewTimer(time.Millisecond * 100)
	select {
	case <-timer.C:
		log.Err(fmt.Errorf("outbound timeout"))
		return
	case r.outbound <- marshal:
		timer.Stop()
		return
	}

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

	log.Event("Unit '%s' @ %s connected", unit.Name, r.address)

	r.connection = conn

	return nil

}

func (r *RemoteUnit) listen() {
	for {
		_, data, err := r.connection.ReadMessage()
		if err != nil {
			return
		}
		r.process(data)
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
