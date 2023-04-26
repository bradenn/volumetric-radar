package pkg

import (
	"bridge/internal/infrastructure/log"
	"encoding/json"
	"fmt"
	"github.com/gorilla/websocket"
	"github.com/mjibson/go-dsp/fft"
	"gonum.org/v1/gonum/dsp/fourier"
	"gonum.org/v1/gonum/dsp/window"
	"math"
	"math/cmplx"
	"os"
	"strings"
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
	FrameDigest = 12500
	FrameRender = 12500 * 2
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
	Pan        float64   `json:"pan"`
	Tilt       float64   `json:"tilt"`
	Channels   []Channel `json:"channels"`
	Metadata   Metadata  `json:"metadata"`
	Phase      []float64 `json:"phase"`
	Distance   []float64 `json:"distance"`
	Rate       float64   `json:"rate"`
	lastChange bool
}

const BufferSize = 1
const IncomingSize = 256 * (1)

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
	start time.Time
	rate  float64

	address string
}

func (r *RemoteUnit) export() ([]byte, error) {
	var unit Unit

	r.display.RLock()
	unit = r.Unit
	r.display.RUnlock()

	marshal, err := json.Marshal(unit)
	if err != nil {
		return []byte{}, err
	}

	return marshal, nil
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

func GenerateChirp(f0, f1, tChirp float64, fs int) []complex128 {
	bw := f1 - f0                                    // bandwidth (Hz)
	nSamples := int(math.Ceil(tChirp * float64(fs))) // number of samples
	dt := tChirp / float64(nSamples)                 // time step (seconds)
	chirp := make([]complex128, nSamples)            // initialize chirp signal

	for n := 0; n < nSamples; n++ {
		t := float64(n) * dt
		k := bw / tChirp                                   // chirp rate
		phi := 2 * math.Pi * (f0*t + 0.5*k*math.Pow(t, 2)) // instantaneous phase

		// Calculate complex chirp signal
		chirp[n] = complex(math.Cos(phi), math.Sin(phi))
	}

	return chirp
}

func MatchedFilter(receivedData, chirpSignal []complex128) []complex128 {
	chirpLen := len(chirpSignal)
	dataLen := len(receivedData)
	outputLen := dataLen - chirpLen + 1
	matchedFilterOutput := make([]complex128, outputLen)

	// Conjugate and time-reverse the chirp signal
	conjTimeReversedChirp := make([]complex128, chirpLen)
	for i, v := range chirpSignal {
		conjTimeReversedChirp[chirpLen-1-i] = cmplx.Conj(v)
	}

	// Perform the matched filtering operation
	for i := 0; i < outputLen; i++ {
		var sum complex128
		for j := 0; j < chirpLen; j++ {
			sum += receivedData[i+j] * conjTimeReversedChirp[j]
		}
		matchedFilterOutput[i] = sum
	}

	return matchedFilterOutput
}
func complexConjugateMultiplication(a, b []complex128) []complex128 {
	result := make([]complex128, len(a))
	for i := range a {
		result[i] = a[i] * cmplx.Conj(b[i])
	}
	return result
}

type MagnitudeIndex struct {
	Magnitude float64
	Index     int
}

func FindBeatFrequency(iqData []complex128, sampleRate float64) float64 {
	// Apply a windowing function (Hanning window) to the I/Q data
	windowedData := window.BlackmanHarrisComplex(iqData)

	// Perform the FFT on the windowed I/Q data
	fftResult := fft.FFT(windowedData)

	findPeakIndex(fftResult)
	// Calculate the beat frequency based on the index of the highest peak
	beatFrequency := float64(findPeakIndex(fftResult)) * sampleRate / float64(len(iqData))

	return beatFrequency
}

func applyHanningWindow(data []complex128) []complex128 {
	windowedData := make([]complex128, len(data))
	N := len(data)

	for n := range data {
		window := 0.5 * (1 - math.Cos(2*math.Pi*float64(n)/float64(N-1)))
		windowedData[n] = complex(real(data[n])*window, imag(data[n])*window)
	}

	return windowedData
}

func findPeaksAboveThreshold(magnitudes []MagnitudeIndex, threshold float64) []MagnitudeIndex {
	peaks := []MagnitudeIndex{}
	for _, mag := range magnitudes {
		if mag.Magnitude > threshold {
			peaks = append(peaks, mag)
		}
	}
	return peaks
}
func CalculateDistance(beatFrequency float64) float64 {
	// Chirp parameters
	bandwidth := 6.1e6     // 200 MHz
	chirpDuration := 25e-3 // 12.5 ms
	speedOfLight := 3e8    // 300,000,000 m/s

	// Calculate slope
	slope := 2 * bandwidth / chirpDuration

	// Calculate round-trip time delay
	timeDelay := beatFrequency / slope

	// Calculate distance
	distance := (speedOfLight * timeDelay) / 2

	return distance
}

func findPeakIndex(signal []complex128) int {
	maxMagnitude := 0.0
	peakIndex := -1
	halfSignalLength := len(signal) / 2
	for i, val := range signal[:halfSignalLength] {
		magnitude := cmplx.Abs(val)
		if magnitude > maxMagnitude {
			maxMagnitude = magnitude
			peakIndex = i
		}
	}
	return peakIndex
}

func CalculateFrequency(index int, sampleRate int, fftSize int) float64 {
	return float64(index) * float64(sampleRate) / float64(fftSize)
}

func findPeaks(fftResult []complex128, windowSize int) []int {
	peaks := []int{}
	n := len(fftResult)

	for i := windowSize; i < n-windowSize; i++ {
		magnitude := cmplx.Abs(fftResult[i])

		isPeak := true
		for j := -windowSize; j <= windowSize; j++ {
			if j == 0 {
				continue
			}

			neighborMagnitude := cmplx.Abs(fftResult[i+j])
			if magnitude <= neighborMagnitude {
				isPeak = false
				break
			}
		}

		if isPeak {
			peaks = append(peaks, i)
		}
	}

	return peaks
}

func removeDCOffset(signal []complex128) []complex128 {
	mean := complex(0, 0)
	for _, val := range signal {
		mean += val
	}
	mean /= complex(float64(len(signal)), 0)

	result := make([]complex128, len(signal))
	for i, val := range signal {
		result[i] = val - mean
	}
	return result
}
func (r *RemoteUnit) digest() {
	r.index = (r.index + 1) % BufferSize
	r.mutex.Lock()
	if len(r.buffer[0]) < BufferSize*IncomingSize || len(r.buffer[1]) < BufferSize*IncomingSize {

		r.mutex.Unlock()

		return
	}

	cmp1 := r.buffer[0]
	//h1 := cmp1
	cmp2 := r.buffer[1]
	//h2 := cmp2
	r.mutex.Unlock()

	// 10000 samples/sec
	// 38ms chirp
	// 10 samples/ms
	// 380 samples/chirp (384)

	// 1000 dac vals / s
	//
	//val := 0.0
	//filter := []float64{}
	//
	////37.23636364*16.384
	//
	////4096/64 = 128 points/chirp (64ms)
	//// 256 samples/chirp
	//
	//dacRate := 1.0                      // 500us/update
	//sampleRate := 128.0 * 128.0         // 16384 samples/s
	//samplesPerMs := sampleRate / 1000.0 // 16.384 samples/ms
	//
	//dacStep := 64.0                     // 64 levels per step
	//dacResolution := 4096.0             // steps per chirp
	//dacSteps := dacResolution / dacStep // Number of levels per chirp
	////samplesPerStep := samplesPerMs * dacRate
	//
	//dacDurationMs := dacSteps * dacRate             // Duration of a chirp
	//samplesPerChirp := dacDurationMs * samplesPerMs // samples taken per chirp
	///*	val := 0.0
	//	dx := 1.0 / dacSteps*/
	//for i := 0.0; i < samplesPerChirp; i++ {
	//	filter = append(filter, (samplesPerChirp-i)*(1/samplesPerChirp))
	//}

	unit := Unit{
		Channels: make([]Channel, 2),
		Metadata: r.Unit.Metadata,
		Pan:      r.Unit.Pan,
		Tilt:     r.Unit.Tilt,
	}
	//unit.Channels[0].SignalI = []float64{}
	//unit.Channels[0].SignalQ = []float64{}
	cmp1 = removeDCOffset(cmp1)
	cmp2 = removeDCOffset(cmp2)

	//cmp1 = window.HannComplex(cmp1)
	//cmp2 = window.HannComplex(cmp2)
	//txSignal := linearChirp(24e9, 25.0e-3, 6.86645508e6, 256)
	//
	//chirp := generateSawtoothChirp(1024*20, 12.5e-3, 0, 6.86645508e6, 256)
	//chirp = undersample(chirp, 10)
	//cmp1 = MatchedFilter(cmp1, txSignal)
	//cmp2 = MatchedFilter(cmp2, txSignal)
	phaseDiff := phaseDifference(cmp1, cmp2)
	//aoa := angleOfApproach(phaseDiff, 0.01245606, 8.763)
	unit.Phase = phaseDiff
	unit.Distance = make([]float64, len(phaseDiff))
	for _, c := range cmp1 {

		unit.Channels[0].SignalI = append(unit.Channels[0].SignalI, real(c))
		unit.Channels[0].SignalQ = append(unit.Channels[0].SignalQ, imag(c))
	}

	//fmt.Println(findPeaks(cmp1, 20))
	//unit.Channels[0].SignalI = bandpassFilter(unit.Channels[0].SignalI, 100, 10000, 20*1024, 4)
	for _, c := range cmp2 {
		unit.Channels[1].SignalI = append(unit.Channels[1].SignalI, real(c))
		unit.Channels[1].SignalQ = append(unit.Channels[1].SignalQ, imag(c))
	}

	unit.Channels[0].Spectrum = []float64{}
	unit.Channels[1].Spectrum = []float64{}
	//pf1 := FindBeatFrequency(cmp1, 1024*20)
	//pf2 := FindBeatFrequency(cmp2, 1024*20)

	//unit.Phase = append(unit.Phase, aoa)
	//unit.Distance = append(unit.Phase, 1)
	//fmt.Println(aoa)
	//// 20400
	//dx := (pf1 + pf2) / 2
	//if dx != 0 {
	//	fmt.Println(CalculateDistance(pf1), CalculateDistance(pf2))
	//}
	ff := fourier.NewCmplxFFT(len(cmp1))
	//cmp1_c = hanningWindow(cmp1_c)
	//peak := findPeakIndex(cmp1_c)
	//dist := calculateDistance(peak, 25e-3, 250e6, 299792458)
	//freqs := []float64{}
	//for i := 0; i < len(cmp1); i++ {
	//	freqs = append(freqs, CalculateFrequency(i, 20*1024, len(cmp1_c)))
	//}
	//fmt.Println(freqs[peak])
	//fmt.Println(dist)

	cmp1 = ff.Coefficients(nil, cmp1)
	for _, c := range cmp1[0 : len(cmp1)/2] {
		unit.Channels[0].Spectrum = append(unit.Channels[0].Spectrum, cmplx.Abs(c))
	}

	cmp2 = ff.Coefficients(nil, cmp2)
	for _, c := range cmp2[0 : len(cmp2)/2] {
		unit.Channels[1].Spectrum = append(unit.Channels[1].Spectrum, cmplx.Abs(c))
	}

	//p
	unit.Metadata.Window = BufferSize * IncomingSize
	unit.Rate = r.rate
	//
	unit.Channels[0].SignalI = downsample(unit.Channels[0].SignalI, 1024)
	unit.Channels[0].SignalQ = downsample(unit.Channels[0].SignalQ, 1024)
	//unit.Channels[0].Spectrum = downsample(unit.Channels[0].Spectrum[0:len(unit.Channels[0].Spectrum)/2], 1024)
	unit.Channels[1].SignalI = downsample(unit.Channels[1].SignalI, 1024)
	unit.Channels[1].SignalQ = downsample(unit.Channels[1].SignalQ, 1024)
	//unit.Channels[1].Spectrum = downsample(unit.Channels[1].Spectrum[0:len(unit.Channels[0].Spectrum)/2], 1024)

	r.mutex.Lock()
	//r.buffer[0] = []complex128{}
	//r.buffer[1] = []complex128{}
	r.mutex.Unlock()

	r.display.Lock()
	//unit.lastChange = false
	r.Unit = unit
	r.display.Unlock()

}
func normalizePhaseDifference(phase float64) float64 {
	for phase > math.Pi {
		phase -= 2 * math.Pi
	}
	for phase <= -math.Pi {
		phase += 2 * math.Pi
	}
	return phase
}
func phaseDifference(cmp1 []complex128, cmp2 []complex128) []float64 {
	var p []float64
	for i := 0; i < len(cmp1); i++ {
		p1 := cmp1[i]
		p2 := cmp2[i]
		phase1 := math.Atan2(imag(p1), real(p1))
		phase2 := math.Atan2(imag(p2), real(p2))

		difference := math.Asin(0.2257 * normalizePhaseDifference(phase1-phase2))
		if !math.IsNaN(difference) {
			p = append(p, difference*(180/math.Pi))
		}
	}

	return p
}

func (r *RemoteUnit) tick() {
	r.display.RLock()

	unit := r.Unit
	if unit.lastChange {
		r.display.RUnlock()
		return
	}
	r.Unit.lastChange = true
	r.display.RUnlock()

	marshal, err := json.Marshal(unit)
	if err != nil {
		log.Err(err)
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
func linearChirp(f0, T, B float64, N int) []complex128 {
	chirp := make([]complex128, N)
	k := B / T
	for i := range chirp {
		t := float64(i) * T / float64(N-1)
		f := f0 + 0.5*k*t
		chirp[i] = cmplx.Rect(1, 2*math.Pi*f*t)
	}
	return chirp
}
func complexSinusoid(frequency, phase float64, length int) []complex128 {
	sinusoid := make([]complex128, length)
	for i := range sinusoid {
		t := float64(i)
		sinusoid[i] = cmplx.Rect(1, 2*math.Pi*frequency*t+phase)
	}
	return sinusoid
}

var set = 0

func (r *RemoteUnit) process(data []byte) {
	//if set > 10 {
	//	return
	//}
	//fmt.Println((len(data) - 8*2) / 2)
	incoming := decodeBinaryToFloat64Arrays(data, 256)
	if len(incoming) < 4 {
		//fmt.Println("FAIL", incoming)
		return
	}
	//set++
	//value, err := extractInt64FromBytes(data, (len(data)-1)-16, len(data)-1-8)
	//if err != nil {
	//	fmt.Println(err)
	//}

	adcStart := int64(0)
	adcStop := int64(0)
	//chirpStart := int64(0)
	//chirtpStop := int64(0)
	for i := 0; i < 4; i++ {
		value1, err := extractInt64FromBytes(data, (len(data)-1)-i*8-7, (len(data)-1)-i*8)
		if err != nil {
			fmt.Println(err)
		}
		switch i {
		case 1:
			adcStart = value1
		case 0:
			adcStop = value1
			//case 1:
			//	chirpStart = value1
			//case 0:
			//	chirtpStop = value1
		}
		//fmt.Printf("%d, ", value1)
	}
	r.rate = float64(1000 / ((adcStop - adcStart) / 1000))
	//fmt.Printf("ADC: %dµs Chirp: %dµs\n", adcStop-adcStart, chirtpStop-chirpStart)
	//value1, err := extractInt64FromBytes(data, (len(data)-1)-15, (len(data)-1)-8)
	//if err != nil {
	//	fmt.Println(err)
	//}
	//value2, err := extractInt64FromBytes(data, (len(data)-1)-7, len(data)-1)
	//if err != nil {
	//	fmt.Println(err)
	//}
	//fmt.Println(int64(value2) - int64(value1))

	//fmt.Println(incoming)
	//
	//for _, result := range packet.Results {
	//	array, err := hexStringToIntArray(result)
	//	if err != nil {
	//		return
	//	}
	//	incoming = append(incoming, intArrayToFloatArray(array))
	//}
	r.total += len(incoming[0])
	//
	if time.Since(r.start).Seconds() >= 1 {
		r.rate = float64(r.total) / time.Since(r.start).Seconds()
		//fmt.Println(r.rate)
		r.total = 0
		r.start = time.Now()
	}
	//incoming[0] = normalizeArray(incoming[0])
	//incoming[1] = normalizeArray(incoming[1])
	//incoming[2] = normalizeArray(incoming[2])
	//incoming[3] = normalizeArray(incoming[3])
	var cmp1 []complex128
	var cmp2 []complex128
	//incoming[0] = bandpassFilter(incoming[0], 24e9, 1500, 20*1024, 1)
	for i := range incoming[0] {

		cmp1 = append(cmp1, complex(incoming[0][i], incoming[1][i]))
		cmp2 = append(cmp2, complex(incoming[3][i], incoming[2][i]))
	}
	//cmp1 = removeDCOffset(cmp1)
	//cmp2 = removeDCOffset(cmp1)
	//for _, f := range incoming[4] {
	//	if minHz > f {
	//		minHz = f
	//	}
	//	if maxHz < f {
	//		maxHz = f
	//	}
	//}
	r.mutex.Lock()
	// 9600 s/s 768.04915515 -> 12.4992
	r.buffer[0] = append(r.buffer[0], cmp1...)
	r.buffer[1] = append(r.buffer[1], cmp2...)
	//for j := 0; j < 5; j++ {
	//	incoming[j] = normalizeArray(incoming[j])
	//}
	//
	//r.dsp[0] = append(r.dsp[0], incoming[0]...)
	//r.dsp[1] = append(r.dsp[1], incoming[1]...)
	//r.dsp[2] = append(r.dsp[2], incoming[2]...)
	//r.dsp[3] = append(r.dsp[3], incoming[3]...)
	//r.dsp[4] = append(r.dsp[4], incoming[4]...)

	if len(r.buffer[0]) > IncomingSize*BufferSize {
		r.buffer[0] = r.buffer[0][(len(r.buffer[0]) - (IncomingSize * BufferSize)):]
	}
	if len(r.buffer[1]) > IncomingSize*BufferSize {
		r.buffer[1] = r.buffer[1][(len(r.buffer[1]) - (IncomingSize * BufferSize)):]
	}

	r.mutex.Unlock()
	return
	if len(r.buffer[1]) >= IncomingSize*BufferSize {

		file, err := os.OpenFile("out.csv", os.O_RDWR, os.ModePerm)
		if err != nil {
			fmt.Println("STOP")
			return
		}
		counter := 0
		var csv []string
		csv = append(csv, "t,i1,q1,q2,i2")
		for i := 0; i < BufferSize*IncomingSize; i++ {
			var ln []string
			ln = append(ln, fmt.Sprintf("%d", counter))

			for j := 0; j < 4; j++ {
				ln = append(ln, fmt.Sprintf("%d", int(r.dsp[j][i])))
			}
			counter++
			csv = append(csv, strings.Join(ln, ","))
		}

		_, err = file.WriteString(strings.Join(csv, "\n"))
		if err != nil {
			fmt.Println("STOP")
			return
		}
		file.Close()
		os.Exit(0)
	}
	//r.digest()
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
	//countdown = time.Now()
	go func() {
		ticker := time.NewTicker(time.Microsecond * FrameRender)
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

type UnitMeta struct {
	Pan    float64 `json:"pitch"`
	Tilt   float64 `json:"roll"`
	Buffer string  `json:"buffer"`
}

func (r *RemoteUnit) listen() {
	for {
		if r.connection == nil {
			fmt.Println("CONNECTION NULL")
			return
		}
		t, data, err := r.connection.ReadMessage()
		if err != nil {
			fmt.Println("CONNECTION ERR", err)
			r.connection.Close()
		}
		if t == websocket.BinaryMessage {
			r.process(data)

		} else if t == websocket.TextMessage {
			//fmt.Println(string(data))
			p := UnitMeta{}
			err = json.Unmarshal(data, &p)
			if err != nil {
				continue
			}
			//fmt.Println(p)
			r.Unit.Pan = p.Tilt
			r.Unit.Tilt = p.Pan
		}
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
