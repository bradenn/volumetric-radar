package pkg

import (
	"encoding/json"
	"fmt"
	"github.com/gorilla/websocket"
	fft_ "github.com/mjibson/go-dsp/fft"
	"log"
	"math"
	"math/cmplx"
	"net/http"
	"sync"
	"time"
)

var buffer []complex128

type Remote struct {
	timeStarted time.Time
	conn        *websocket.Conn
	out         *websocket.Conn
	buffer      []uint16
	buffer4     []uint16
	buffer3     []uint16
	buffer2     []uint16
	buffer1     []uint16
	buffer0     []uint16
	wg          sync.WaitGroup

	realBuf  map[int]chan []float64
	mt       sync.RWMutex
	dsp      map[string][]float64
	bufs     map[int][]uint16
	cherries map[int][]float64
	bufChan  chan Buffer
	b        chan []uint16
	c        chan []uint16
	d        chan []uint16
	e        chan []uint16
	f        chan []uint16
	stream   chan []byte
	idx      int
}

const (
	frameWidth = 100
)

func findLocalMaxima(signal []float64, threshold float64) []int {
	var localMaxima []int

	for i := 1; i < len(signal)-1; i++ {
		if signal[i] > signal[i-1] && signal[i] > signal[i+1] && signal[i] >= threshold {
			localMaxima = append(localMaxima, i)
		}
	}

	return localMaxima
}

func (s *Remote) serve() {

	http.HandleFunc("/", s.runWs)

	err := http.ListenAndServe("0.0.0.0:5500", nil)
	if err != nil {
		log.Println("read:", err)
	}

}

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

func (s *Remote) runWs(w http.ResponseWriter, r *http.Request) {
	var err error
	var conn *websocket.Conn
	conn, err = upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println("read:", err)
		return
	}
	defer conn.Close()

	for {
		_, _, err = conn.ReadMessage()
		if err != nil {
			log.Println("read:", err)
			conn.Close()
			continue
		}

		ticker := time.NewTicker(time.Millisecond * 100)
		defer ticker.Stop()
		for {
			select {
			case _ = <-ticker.C:
				s.realBuf[0] <- []float64{}
				s.realBuf[1] <- []float64{}
				s.realBuf[2] <- []float64{}
				s.realBuf[3] <- []float64{}
				s.process()
			}
		}

	}
}

func (s *Remote) output() {
	p := Packet{}
	var err error
	out, err := json.Marshal(s.dsp)
	if err != nil {
		log.Println("dsp arr:", err)
	}
	//p.Payload = string(out)
	out, err = json.Marshal(p)
	if err != nil {
		log.Println("read:", err)
	}
	//fmt.Println(string(out))
	s.stream <- out
}

func lerp(a, b, t float64) float64 {
	return a*(1-t) + b*t
}

func calculateSpeed(freq float64, radarFreq float64) float64 {
	speedOfLight := 299792458.0 // meters per second
	frequencyShift := 1 / (freq - radarFreq)
	speed := (frequencyShift * speedOfLight) / (1 / radarFreq)
	return speed
}

type Channel struct {
	SignalI []float64 `json:"signalI"`
	SignalQ []float64 `json:"signalQ"`
	Phase   []float64 `json:"phase"`

	Spectrum    []float64 `json:"spectrum"`
	Frequency   float64   `json:"frequency"`
	Frequencies []float64 `json:"frequencies"`
	Peaks       []float64 `json:"peaks"`
}

func NewChannel(I []uint16, Q []uint16, frequency float64) (*Channel, error) {
	// Initialize Channel
	c := Channel{
		SignalI:   []float64{},
		SignalQ:   []float64{},
		Frequency: frequency,
	}
	// Convert in-phase input to float
	for i := range I {
		c.SignalI = append(c.SignalI, float64(I[i]))
	}
	// Convert quadrature input to float
	for i := range Q {
		c.SignalQ = append(c.SignalQ, float64(Q[i]))
	}

	return &c, nil
}

func (c *Channel) FFT(c2 Channel) []complex128 {
	cmp := c.complex()
	cmp1 := c2.complex()
	if len(cmp) != len(cmp1) {
		return []complex128{}
	}
	fftOut := fft_.FFT2([][]complex128{cmp, cmp1})
	res := fftOut[0]

	c.Spectrum = []float64{}
	for i := range res {
		c.Spectrum = append(c.Spectrum, cmplx.Abs(res[i]))
	}

	mx := -1.0
	for i := range c.Spectrum {
		if c.Spectrum[i] > mx {
			mx = c.Spectrum[i]
		}
	}

	c.Peaks = []float64{}
	peakLocations := findLocalMaxima(c.Spectrum, mx/2)
	for i := range peakLocations {
		c.Peaks = append(c.Peaks, float64(peakLocations[i]))
	}

	// Generate frequency bins
	c.Frequencies = []float64{}
	for i := 0; i < len(res); i++ {
		c.Frequencies = append(c.Frequencies, (float64(i)*c.Frequency)/float64(len(c.SignalI)))
	}

	return res
}
func getDisplayArrayWithTime(buffer [][]float64, pulseTime, pulseDuration, p float64, displayLength int) []float64 {
	// Create a new display array with the specified length
	display := make([]float64, displayLength)

	// Check that buffer and display slices are not empty
	if len(buffer) == 0 || len(buffer[0]) == 0 || len(display) == 0 {
		return display
	}

	// Calculate the pulse index based on the time scale and pulse duration
	pulseIndex := int(math.Round(pulseTime / p))

	// Loop through all frames in the buffer
	for frameIndex := 0; frameIndex < len(buffer); frameIndex++ {
		// Check that buffer slice contains at least frameIndex + 1 elements
		if len(buffer[frameIndex]) == 0 {
			continue
		}

		// Calculate the start and end indices for the pulse in the display slice
		pulseStart := (pulseIndex + frameIndex*len(buffer[frameIndex])) % len(display)
		pulseEnd := (pulseStart + int(math.Round(pulseDuration/p))) % len(display)

		// Copy data from buffer array to display array starting at pulseStart
		if pulseEnd > pulseStart {
			copy(display[pulseStart:pulseEnd], buffer[frameIndex])
		} else {
			copy(display[pulseStart:], buffer[frameIndex][:len(display)-pulseStart])
			copy(display[:pulseEnd], buffer[frameIndex][len(display)-pulseStart:])
		}
	}

	return display
}

func (c *Channel) complex() []complex128 {
	var cmp []complex128
	for i := range c.SignalI {
		cmp = append(cmp, complex(c.SignalI[i], c.SignalQ[i]))
	}
	return cmp
}

func (c *Channel) marshal() ([]byte, error) {
	return json.Marshal(c)
}

func detectPulse(buffer []float64) int {
	// Find the maximum value in the buffer
	if len(buffer) <= 0 {
		return 0
	}
	maxVal := 0.0
	maxIndex := 0
	for i, val := range buffer {
		if val > maxVal {
			maxVal = val
			maxIndex = i
		}
	}

	// Look for the first point after the maximum value where the signal drops below a threshold
	threshold := maxVal / 2
	for i := maxIndex; i < len(buffer); i++ {
		if buffer[i] < threshold {
			return i
		}
	}

	// If no threshold is found, search backwards from the maximum value instead
	for i := maxIndex; i >= 0; i-- {
		if buffer[i] < threshold {
			return i
		}
	}

	// If we still haven't found a pulse, return a default value
	return 0
}

func (s *Remote) sumData(source int) []float64 {
	block := s.cleanBuffers(source, 10)
	var out []float64
	if len(block) <= 0 {
		for i := 0; i < frameWidth; i++ {
			out = append(out, 0)
		}
		return out
	}
	for _, float64s := range block {
		for i2 := range float64s {
			out = append(out, float64s[i2])
		}
	}

	return out
}
func (s *Remote) cleanBuffers(source int, limit int) [][]float64 {
	var out [][]float64
	for len(s.realBuf[source]) < limit-1 {

	}
	for {

		select {
		case res := <-s.realBuf[source]:
			var arr [][]float64
			arr = append(arr, res)
			out = append(arr, out...)
			limit -= 1

		default:
			return out
		}
	}
}

func hanningWindow(n int) []float64 {
	window := make([]float64, n)
	for i := 0; i < n; i++ {
		window[i] = 0.5 * (1 - math.Cos(2*math.Pi*float64(i)/float64(n-1)))
	}
	return window
}
func applyHanningWindow(signal []complex128) []complex128 {
	n := len(signal)
	window := hanningWindow(n)
	for i := 0; i < n; i++ {
		signal[i] = signal[i] * complex(window[i], 0)
	}
	return signal
}
func matchedFilter(signal []complex128, pulse []complex128) []complex128 {
	// calculate the complex conjugate of the pulse signal
	pulseConj := make([]complex128, len(pulse))
	for i, val := range pulse {
		pulseConj[i] = cmplx.Conj(val)
	}

	// pad the signal with zeros to ensure it keeps the same length
	paddedSignal := make([]complex128, len(signal)+len(pulse)-1)
	copy(paddedSignal, signal)

	// perform the matched filter
	filteredSignal := make([]complex128, len(signal))
	for i := 0; i < len(signal); i++ {
		var sum complex128
		for j := 0; j < len(pulse); j++ {
			sum += paddedSignal[i+j] * pulseConj[j]
		}
		filteredSignal[i] = sum
	}

	return filteredSignal
}
func (s *Remote) process() {

	channel0 := Channel{}
	channel1 := Channel{}

	channel0.SignalI = []float64{}
	channel0.SignalQ = []float64{}
	channel1.SignalI = []float64{}
	channel1.SignalQ = []float64{}

	channel0.SignalI = append(channel0.SignalI, <-s.realBuf[0]...)
	channel0.SignalQ = append(channel0.SignalQ, <-s.realBuf[1]...)
	channel1.SignalI = append(channel1.SignalI, <-s.realBuf[2]...)
	channel1.SignalQ = append(channel1.SignalQ, <-s.realBuf[3]...)

	var cmp1 []complex128
	var cmp2 []complex128

	for i := range channel0.SignalI {
		cmp1 = append(cmp1, complex(channel0.SignalI[i], channel0.SignalQ[i]))
	}
	for i := range channel1.SignalI {
		cmp2 = append(cmp2, complex(channel1.SignalI[i], channel1.SignalQ[i]))
	}
	channel0.Phase = []float64{}
	for i := range channel0.SignalI {
		dx := cmplx.Phase(cmp2[i]) - cmplx.Phase(cmp1[i])
		channel0.Phase = append(channel0.Phase, math.Asin(dx*0.2257))
	}
	match := []complex128{
		1 - 0i, 1 - 0i, 1 - 0i, 1 - 0i, 1 - 0i,
		1 - 0i, 1 - 0i, 1 - 0i, 1 - 0i, 1 - 0i,
		0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i,
		0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i,
		0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i,
		0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i, 0 + 0i,
		0 + 0i, 0 + 0i,
	}
	//Apply Hanning Window
	cmp1 = applyHanningWindow(matchedFilter(cmp1, match))
	cmp2 = applyHanningWindow(matchedFilter(cmp1, match))

	// Apply FFT
	fft1 := fft_.FFT(cmp1)
	channel0.Spectrum = []float64{}
	for i := range fft1 {
		channel0.Spectrum = append(channel0.Spectrum, cmplx.Abs(fft1[i]))
	}

	fft2 := fft_.FFT(cmp2)
	channel1.Spectrum = []float64{}
	for i := range fft2 {
		channel1.Spectrum = append(channel1.Spectrum, cmplx.Abs(fft2[i]))
	}

	mx0 := -1.0
	for i := range channel0.Spectrum {
		if channel0.Spectrum[i] > mx0 {
			mx0 = channel0.Spectrum[i]
		}
	}
	mx1 := -1.0
	for i := range channel1.Spectrum {
		if channel1.Spectrum[i] > mx1 {
			mx1 = channel1.Spectrum[i]
		}
	}

	channel0.Peaks = []float64{}
	peakLocations := findLocalMaxima(channel0.Spectrum, mx0/4)
	for i := range peakLocations {
		channel0.Peaks = append(channel0.Peaks, float64(peakLocations[i]))
	}
	channel1.Peaks = []float64{}
	peakLocations1 := findLocalMaxima(channel1.Spectrum, mx1/4)
	for i := range peakLocations1 {
		channel1.Peaks = append(channel1.Peaks, float64(peakLocations1[i]))
	}

	// Generate frequency bins
	channel0.Frequencies = []float64{}
	for i := 0; i < len(channel0.Spectrum); i++ {
		channel0.Frequencies = append(channel1.Frequencies, (float64(i)*(80000/8))/float64(len(channel0.SignalI)))
	}

	// Generate frequency bins
	channel1.Frequencies = []float64{}
	for i := 0; i < len(channel1.Spectrum); i++ {
		channel1.Frequencies = append(channel1.Frequencies, (float64(i)*(80000/8))/float64(len(channel1.SignalI)))
	}

	//if len(fft1) > 0 {
	//
	//	ift0 := fft.IFFT(fft1)
	//	for i := 0; i < len(channel1.Spectrum); i++ {
	//		if len(ift0) <= 0 {
	//			break
	//		}
	//		channel0.Spectrum[i] = cmplx.Abs(ift0[i])
	//	}
	//
	//	fft.IFFT(fft2)
	//}
	arr := []Channel{channel0, channel1}
	marshal, err := json.Marshal(arr)
	if err != nil {
		return
	}
	select {
	case s.stream <- marshal:
	default:
	}
}

type Buffer struct {
	Source  int      `json:"source"`
	Channel int      `json:"channel"`
	Buffer  []uint16 `json:"buffer"`
}

func (s *Remote) run() {
	for {
		select {
		case buf := <-s.bufChan:

			var out []float64
			for i := 0; i < len(buf.Buffer); i++ {
				out = append(out, float64(buf.Buffer[i]))
			}

			select {
			case s.realBuf[buf.Source] <- out:
				break
			default:
				break
			}
			//s.wg.Done()
			break
		}

	}
}

func (s *Remote) srv() {
	for income := range s.stream {
		if s.out != nil {
			//fmt.Println(income)
			_ = s.out.SetWriteDeadline(time.Now().Add(time.Millisecond * 100))
			_ = s.out.WriteMessage(websocket.TextMessage, income)
		}
	}
}

type Runnable struct {
}

type Packet struct {
	Results []string `json:"results"`
}

func NewServer(host string, buf []complex128) *Remote {
	buffer = buf
	srv := &Remote{
		dsp: map[string][]float64{},
	}
	srv.begin()
	var err error
	srv.conn, _, err = websocket.DefaultDialer.Dial(host, nil)
	if err != nil {
		log.Println("read:", err)

	}
	err = srv.conn.WriteMessage(websocket.TextMessage, []byte("Ping!"))
	if err != nil {
		log.Println("read:", err)
	}

	return srv

}

func (s *Remote) begin() {
	s.bufChan = make(chan Buffer, 128)
	s.stream = make(chan []byte, 64)
	s.bufs = map[int][]uint16{}
	s.cherries = map[int][]float64{}
	s.realBuf = map[int]chan []float64{}
	const fluidBufSize = 100
	s.realBuf[0] = make(chan []float64, fluidBufSize)
	s.realBuf[1] = make(chan []float64, fluidBufSize)
	s.realBuf[2] = make(chan []float64, fluidBufSize)
	s.realBuf[3] = make(chan []float64, fluidBufSize)
	s.realBuf[4] = make(chan []float64, fluidBufSize)
	s.wg = sync.WaitGroup{}
	s.mt = sync.RWMutex{}
	s.idx = 0

	go func() {
		s.serve()
	}()
	go func() {
		s.srv()
	}()
	for {
	}
	msg := 0
	fmt.Printf("%03d", 0)
	reset := time.Now()
	for {
		_, message, err := s.conn.ReadMessage()
		if err != nil {
			log.Println("read:", err)
			s.conn.Close()
			return
		}

		p := Packet{}
		err = json.Unmarshal(message, &p)
		if err != nil {
			fmt.Printf("Error: %s\n", err)
			continue
		}

		//s.bufChan <- Buffer{
		//	Buffer: unpackArray(p.Ch4),
		//	Source: 4,
		//}

		s.process()
		msg = (msg + 1) % 1000
		if msg == 0 {
			reset = time.Now()
		}
		fmt.Printf("%03f\n", float64(msg)/time.Since(reset).Seconds())

	}

	fmt.Println("Loop exit")
}
