package pkg

import (
	"bridge/internal/infrastructure/log"
	"bridge/internal/pkg/dsp"
	"encoding/json"
	"fmt"
	"github.com/gorilla/websocket"
	"github.com/mjibson/go-dsp/fft"
	"gonum.org/v1/gonum/dsp/window"
	"math"
	"math/cmplx"
	"sort"
	"sync"
	"time"
)

type Chirp struct {
	Prf        int `json:"prf"`
	Duration   int `json:"duration"`
	Steps      int `json:"steps"`
	Padding    int `json:"padding"`
	Resolution int `json:"resolution"`
}

type Sampling struct {
	Frequency   int `json:"frequency"`
	Samples     int `json:"samples"`
	Attenuation int `json:"attenuation"`
}

type Metadata struct {
	Name      string   `json:"name"`
	Mac       string   `json:"mac"`
	Base      int64    `json:"base"`
	XFov      int      `json:"xFov"`
	YFov      int      `json:"yFov"`
	Enabled   int      `json:"enabled"`
	Audible   int      `json:"audible"`
	Gyro      int      `json:"gyro"`
	Connected bool     `json:"connected"`
	Sampling  Sampling `json:"sampling"`
	Chirp     Chirp    `json:"chirp"`
}

type UnitData struct {
	Name string `json:"name"`
	Mac  string `json:"mac"`
	Base int    `json:"base"`
	XFov int    `json:"xFov"`
	YFov int    `json:"yFov"`
	Adc  struct {
		Chirp     Chirp   `json:"chirp"`
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
	FrameDigest = 25000
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
	Rate float64 `json:"rate"`
}

type Unit struct {
	Rssi        float64   `json:"rssi"`
	Temperature float64   `json:"temperature"`
	Pan         float64   `json:"pan"`
	Tilt        float64   `json:"tilt"`
	Channels    []Channel `json:"channels"`
	Metadata    Metadata  `json:"metadata"`
	Phase       []float64 `json:"phase"`
	Distance    []float64 `json:"distance"`
	Rate        float64   `json:"rate"`
	lastChange  bool
}

const BufferSize = 1
const IncomingSize = 256 * 16

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

	address   string
	timerStop chan bool
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

// NormalizeArray normalizes the input complex array.
func NormalizeArray(arr []complex128) {
	var maxMagnitude float64
	for _, v := range arr {
		magnitude := cmplx.Abs(v)
		if magnitude > maxMagnitude {
			maxMagnitude = magnitude
		}
	}

	for i := range arr {
		arr[i] /= complex(maxMagnitude, 0)
	}
}

// MatchedFilter computes the matched filter of the input signal with the given chirp filter.
func MatchedFilter(signal, chirpFilter []complex128) []complex128 {
	// Normalize the input signal and chirp filter.
	NormalizeArray(signal)
	NormalizeArray(chirpFilter)

	N := len(signal)
	M := len(chirpFilter)
	convolutionLen := N - M + 1
	result := make([]complex128, convolutionLen)

	// Compute the complex conjugate of the chirp filter.
	conjChirpFilter := make([]complex128, M)
	for i, value := range chirpFilter {
		conjChirpFilter[i] = cmplx.Conj(value)
	}

	// Perform convolution.
	for i := 0; i < convolutionLen; i++ {
		accumulator := complex(0, 0)
		for j := 0; j < M; j++ {
			accumulator += signal[i+j] * conjChirpFilter[M-1-j]
		}
		result[i] = accumulator
	}

	return result
}
func complexConjugateMultiplication(a, b []complex128) []complex128 {
	result := make([]complex128, len(a))
	for i := range a {
		result[i] = a[i] * cmplx.Conj(b[i])
	}
	return result
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
func GenerateFMCWSignalShape(duration float64, sampleRate float64) []complex128 {
	numSamples := int(math.Round(duration * sampleRate))
	signal := make([]complex128, numSamples)

	halfDuration := duration / 2.0
	//numHalfCycles := int(math.Round(halfDuration * sampleRate))

	for i := 0; i < numSamples; i++ {
		t := float64(i) / sampleRate
		tInChirp := math.Mod(t, halfDuration)

		// Generate triangle waveform shape
		triangleWave := 2*math.Abs((tInChirp/halfDuration)-0.5) - 0.5

		// Calculate frequency based on triangle waveform
		frequency := sampleRate * 0.1 * triangleWave

		phase := 2.0 * math.Pi * frequency * t
		signal[i] = cmplx.Rect(1.0, phase)
	}

	return signal
}
func MedianFilter(signal []complex128, windowSize int) []complex128 {
	n := len(signal)
	filteredSignal := make([]complex128, n)
	padding := windowSize / 2

	for i := 0; i < n; i++ {
		start := i - padding
		if start < 0 {
			start = 0
		}

		end := i + padding
		if end > n {
			end = n
		}

		window := make([]complex128, end-start)
		copy(window, signal[start:end])

		// Sort the window samples based on their magnitudes
		sort.Slice(window, func(a, b int) bool {
			return cmplx.Abs(window[a]) < cmplx.Abs(window[b])
		})

		// Replace the current sample with the median value
		filteredSignal[i] = window[len(window)/2]
	}

	return filteredSignal
}

func GenerateFMCWSignal(duration float64, bandwidth, sampleRate float64) []complex128 {
	numSamples := int(math.Round(duration * sampleRate))
	signal := make([]complex128, numSamples)

	halfDuration := duration / 2.0
	chirpRate := bandwidth / halfDuration

	for i := 0; i < numSamples; i++ {
		t := float64(i) / sampleRate
		tInChirp := math.Mod(t, halfDuration)

		// Calculate frequency based on triangle waveform.
		var frequency float64
		if tInChirp < halfDuration/2 {
			frequency = chirpRate * tInChirp
		} else {
			frequency = bandwidth - chirpRate*(tInChirp-halfDuration/2)
		}

		phase := 2.0 * math.Pi * frequency * t
		signal[i] = cmplx.Rect(1.0, phase)
	}

	return signal
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
		Channels:    make([]Channel, 2),
		Metadata:    r.Unit.Metadata,
		Pan:         r.Unit.Pan,
		Tilt:        r.Unit.Tilt,
		Temperature: r.Unit.Temperature,
		Rssi:        r.Unit.Rssi,
	}
	//unit.Channels[0].SignalI = []float64{}
	//unit.Channels[0].SignalQ = []float64{}
	//
	//ff := fourier.NewCmplxFFT(len(cmp1))
	//cmp1 = ff.Coefficients(nil, cmp1)
	//cmp2 = ff.Coefficients(nil, cmp2)
	//cmp1 = window.BlackmanHarrisComplex(cmp1)
	//cmp2 = window.BlackmanHarrisComplex(cmp2)
	////
	//cmp1 = ff.Sequence(nil, cmp1)
	//cmp1 = ff.Sequence(nil, cmp2)
	//txSignal := linearChirp(24e9, 25.0e-3, 6.86645508e6, 256)
	//
	// 125/4096
	//chirp := GenerateChirp(0, 6.103516e6, 12.5e-3, 1024*20)
	//chirp = undersample(chirp, 10)
	cmp1 = removeDCOffset(cmp1)
	cmp2 = removeDCOffset(cmp2)
	//
	var filter []complex128
	div := 100.0
	for i := 0; i < 405; i++ {
		m := div / 2.0
		v := m - math.Abs(m-float64(i))
		v2 := m - math.Abs(m-(math.Mod(float64(i)-div/2, div)))
		filter = append(filter, complex(v, v2))
	}
	//
	//// ((500/4096)*2.5)*80
	//w := 24
	//cmp1 = MedianFilter(cmp1, w)
	//cmp2 = MedianFilter(cmp2, w)
	//

	//filter := GenerateFMCWSignal(12.5e-3, 6.10351562e6, 20480)
	cmp1 = MatchedFilter(cmp1, filter)
	cmp2 = MatchedFilter(cmp2, filter)

	phaseDiff := phaseDifference(cmp1, cmp2)
	//aoa := angleOfApproach(phaseDiff, 0.01245606, 8.763)

	degrees := 81.0
	subdegrees := 16.0
	totalDeg := degrees * subdegrees
	bins := make([]float64, int(math.Floor(totalDeg)))

	for _, f := range phaseDiff {
		if f >= -40 && f <= 40 {
			f *= subdegrees
			idx := int(math.Floor(f) + (totalDeg / 2.0))
			bins[idx]++
		}

	}

	bins = movingAverageFilterFloat(bins, 20)
	unit.Phase = bins

	unit.Distance = make([]float64, len(bins))
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
	//ff := fourier.NewCmplxFFT(len(cmp1))
	////cmp1_c = hanningWindow(cmp1_c)
	////peak := findPeakIndex(cmp1_c)
	////dist := calculateDistance(peak, 25e-3, 250e6, 299792458)
	////freqs := []float64{}
	////for i := 0; i < len(cmp1); i++ {
	////	freqs = append(freqs, CalculateFrequency(i, 20*1024, len(cmp1_c)))
	////}
	////fmt.Println(freqs[peak])
	////fmt.Println(dist)
	//
	//cmp1 = ff.Coefficients(nil, cmp1)
	//for _, c := range cmp1[0 : len(cmp1)/2] {
	//	unit.Channels[0].Spectrum = append(unit.Channels[0].Spectrum, cmplx.Abs(c))
	//}
	//
	//cmp2 = ff.Coefficients(nil, cmp2)
	//for _, c := range cmp2[0 : len(cmp2)/2] {
	//	unit.Channels[1].Spectrum = append(unit.Channels[1].Spectrum, cmplx.Abs(c))
	//}

	//p

	unit.Rate = r.rate
	//
	unit.Channels[0].SignalI = dsp.DownSample(unit.Channels[0].SignalI, 1024)
	unit.Channels[0].SignalQ = dsp.DownSample(unit.Channels[0].SignalQ, 1024)
	//unit.Channels[0].Spectrum = downsample(unit.Channels[0].Spectrum[0:len(unit.Channels[0].Spectrum)/2], 1024)
	unit.Channels[1].SignalI = dsp.DownSample(unit.Channels[1].SignalI, 1024)
	unit.Channels[1].SignalQ = dsp.DownSample(unit.Channels[1].SignalQ, 1024)
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
	for phase < -math.Pi {
		phase += 2 * math.Pi
	}
	return phase
}

func phaseDifference(cmp1 []complex128, cmp2 []complex128) []float64 {
	var p []float64
	for i := 0; i < len(cmp1); i++ {
		p1 := cmp1[i]
		p2 := cmp2[i]
		phase1 := cmplx.Phase(p1)
		phase2 := cmplx.Phase(p2)

		dx := phase1 - phase2
		if dx > math.Pi || dx < -math.Pi {
			continue
		}
		difference := math.Asin(0.2257 * dx)
		if !math.IsNaN(difference) {
			p = append(p, difference*(180.0/math.Pi))
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

func movingAverageFilterFloat(data []float64, windowSize int) []float64 {
	filteredData := make([]float64, len(data))

	for i := 0; i < len(data); i++ {
		sum := float64(0)
		count := 0

		for j := i - windowSize; j <= i+windowSize; j++ {
			if j >= 0 && j < len(data) {
				sum += data[j]
				count++
			}
		}

		filteredData[i] = sum / float64(count)
	}

	return filteredData
}

func movingAverageFilter(data []complex128, windowSize int) []complex128 {
	filteredData := make([]complex128, len(data))

	for i := 0; i < len(data); i++ {
		sum := complex128(0)
		count := 0

		for j := i - windowSize; j <= i+windowSize; j++ {
			if j >= 0 && j < len(data) {
				sum += data[j]
				count++
			}
		}

		filteredData[i] = sum / complex(float64(count), 0)
	}

	return filteredData
}

func (r *RemoteUnit) process(data []byte) {

	incoming := decodeBinaryToFloat64Arrays(data, ((len(data)-16)/4)/2)
	if len(incoming) < 4 {
		//fmt.Println("FAIL", len(incoming))
		return
	}

	adcStart := int64(0)
	adcStop := int64(0)
	//chirpStart := int64(0)
	//chirtpStop := int64(0)
	for i := 0; i < 2; i++ {
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

	r.total += len(incoming[0])

	if time.Since(r.start).Seconds() >= 1 {
		r.rate = float64(r.total) / time.Since(r.start).Seconds()
		//fmt.Println(r.rate)
		r.total = 0
		r.start = time.Now()
	}

	var cmp1 []complex128
	var cmp2 []complex128

	//incoming[0] = normalizeArray(incoming[0])
	//incoming[1] = normalizeArray(incoming[1])
	//incoming[2] = normalizeArray(incoming[2])
	//incoming[3] = normalizeArray(incoming[3])

	for i := range incoming[0] {

		cmp1 = append(cmp1, complex(incoming[0][i], incoming[1][i]))
		cmp2 = append(cmp2, complex(incoming[3][i], incoming[2][i]))
	}
	//cmp1 = dsp.RemoveDCOffset(cmp1, complex(1250, 1250))
	//cmp2 = dsp.RemoveDCOffset(cmp2, complex(1250, 1250))

	//

	r.mutex.Lock()

	r.buffer[0] = append(r.buffer[0], cmp1...)
	r.buffer[1] = append(r.buffer[1], cmp2...)

	if len(r.buffer[0]) > IncomingSize*BufferSize {
		r.buffer[0] = r.buffer[0][(len(r.buffer[0]) - (IncomingSize * BufferSize)):]
	}

	if len(r.buffer[1]) > IncomingSize*BufferSize {
		r.buffer[1] = r.buffer[1][(len(r.buffer[1]) - (IncomingSize * BufferSize)):]
	}

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
		r.timerStop <- true
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
	unit := Metadata{}
	err = json.Unmarshal(data, &unit)
	if err != nil {
		return err
	}
	// Metadata converting
	r.display.Lock()
	r.Unit.Metadata = unit
	r.Unit.Metadata.Connected = true
	r.display.Unlock()
	r.connection = conn
	log.Event("Unit '%s' @ %s connected", unit.Name, r.address)
	//countdown = time.Now()
	go func() {
		render := time.NewTicker(time.Microsecond * FrameRender)
		digest := time.NewTicker(time.Microsecond * FrameDigest)
		defer digest.Stop()
		defer render.Stop()
		for {
			select {
			case <-r.timerStop:
				return
			case _ = <-render.C:
				r.tick()
			case _ = <-digest.C:
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
	Pan         float64 `json:"pitch"`
	Tilt        float64 `json:"roll"`
	Buffer      string  `json:"buffer"`
	Temperature float64 `json:"temperature"`
	Rssi        float64 `json:"rssi"`
}

func (r *RemoteUnit) listen() {
	defer func() {
		r.connection.Close()
		r.done <- true
		r.timerStop <- true
	}()
	for {
		if r.connection == nil {
			fmt.Println("CONNECTION NULL")
			return
		}
		t, data, err := r.connection.ReadMessage()
		if err != nil {
			fmt.Println("CONNECTION ERR", err)
			return
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
			r.Unit.Temperature = p.Temperature
			r.Unit.Rssi = p.Rssi
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
			Metadata: Metadata{},
		},
		done:      make(chan bool),
		timerStop: make(chan bool),
		outbound:  out,
		index:     0,
		buffer:    map[int][]complex128{},
		velocity:  map[int][]float64{},
		dsp:       map[int][]float64{},
		address:   addr,
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
