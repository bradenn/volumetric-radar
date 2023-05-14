package pkg

import (
	"bridge/internal/infrastructure/log"
	"bridge/internal/pkg/dsp"
	"bytes"
	"encoding/json"
	"fmt"
	"github.com/go-chi/chi"
	"github.com/go-chi/cors"
	"github.com/gorilla/websocket"
	"gonum.org/v1/gonum/dsp/fourier"
	"io"
	"math"
	"math/cmplx"
	"net/http"
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
	Enabled   int      `json:"enable"`
	Audible   int      `json:"audible"`
	Rate      float64  `json:"rate"`
	Duration  float64  `json:"duration"`
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
	Duration    float64   `json:"duration"`
	Rate        float64   `json:"rate"`
	Samples     float64   `json:"samples"`
	lastChange  bool
}

const BufferSize = 1
const IncomingSize = 16 * 512

type RemoteUnit struct {
	Unit Unit

	mutex   sync.RWMutex
	display sync.RWMutex

	buffer   map[int][]complex128
	velocity map[int][]float64
	spec     map[int][][]complex128

	dsp map[int][]float64

	index int

	connection *websocket.Conn
	outbound   chan []byte
	done       chan bool

	total    int
	start    time.Time
	duration float64
	rate     float64

	threshold []float64

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

func extractChirpDistances(data []complex128, sampleRate float64, sweepBandwidth float64, sweepTime float64, speedOfLight float64) []float64 {
	// Number of samples per chirp
	samplesPerChirp := int(sweepTime * sampleRate)

	// Number of chirps in the data
	numChirps := len(data) / samplesPerChirp

	// Prepare FFT
	fft := fourier.NewCmplxFFT(samplesPerChirp)

	distances := make([]float64, numChirps)

	for i := 0; i < numChirps; i++ {
		chirpData := data[i*samplesPerChirp : (i+1)*samplesPerChirp]

		// Perform FFT on the dechirped signal
		fftData := fft.Coefficients(nil, chirpData)

		// Find the index with the highest amplitude
		peakIndex := 0
		peakAmplitude := 0.0

		for j := 1; j < len(fftData)/2; j++ {
			amplitude := cmplx.Abs(fftData[j])
			if amplitude > peakAmplitude {
				peakIndex = j
				peakAmplitude = amplitude
			}
		}

		// Calculate the distance from the peak frequency
		peakFrequency := float64(peakIndex) * sampleRate / float64(samplesPerChirp)
		distance := (speedOfLight * sweepTime * peakFrequency) / (2 * sweepBandwidth)
		distances[i] = distance
	}

	return distances
}

//func detectChirpPeaks(data []complex128, threshold float64) []int {
//	peakIndices := []int{}
//	for i := 1; i < len(data)-1; i++ {
//		amplitude := cmplx.Abs(data[i])
//		if amplitude > threshold && amplitude > cmplx.Abs(data[i-1]) && amplitude >= cmplx.Abs(data[i+1]) {
//			peakIndices = append(peakIndices, i)
//		}
//	}
//	return peakIndices
//}
//
func estimateDistance(data []complex128, peakIndex int, sampleRate float64, sweepBandwidth float64, sweepTime float64, speedOfLight float64) float64 {
	frequency := float64(peakIndex) * sampleRate / float64(len(data))
	distance := (speedOfLight * sweepTime * frequency) / (2 * sweepBandwidth)
	return distance
}

func CalculateDistance(beatFrequency float64) float64 {
	// Chirp parameters
	bandwidth := 50000000.0  // 200 MHz
	chirpDuration := 12.5e-3 // 12.5 ms
	speedOfLight := 3e8      // 300,000,000 m/s

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

func findPeaks(fftResult []complex128, windowSize int) ([]float64, []float64) {
	peaks := []float64{}
	freqs := []float64{}
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
			peaks = append(peaks, float64(i))
			freqs = append(freqs, CalculateFrequency(i, 20480, len(fftResult)))

		}
	}

	return peaks, freqs
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
func applyWindow(samples []complex128, winFunc func(int) []float64) {
	n := len(samples)
	w := winFunc(n)
	for i := 0; i < n; i++ {
		samples[i] *= complex(w[i], 0)
	}
}
func detectChirpStartTimes(data []complex128, threshold float64) []int {
	startTimes := []int{}
	previousAmplitude := cmplx.Abs(data[0])

	for i := 1; i < len(data); i++ {
		currentAmplitude := cmplx.Abs(data[i])
		derivative := currentAmplitude - previousAmplitude

		if derivative > threshold {
			startTimes = append(startTimes, i)
		}

		previousAmplitude = currentAmplitude
	}

	return startTimes
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

func (r *RemoteUnit) digest() {
	r.index = (r.index + 1) % BufferSize
	// Skip rendering if the frame buffer size has not been met
	r.mutex.Lock()
	if len(r.buffer[0]) < BufferSize*IncomingSize || len(r.buffer[1]) < BufferSize*IncomingSize {
		r.mutex.Unlock()
		return
	}
	// Copy the active buffer to the frame buffer
	cmp1 := r.buffer[0]
	cmp2 := r.buffer[1]
	r.mutex.Unlock()
	//
	unit := Unit{
		Channels:    make([]Channel, 2),
		Metadata:    r.Unit.Metadata,
		Pan:         r.Unit.Pan,
		Tilt:        r.Unit.Tilt,
		Temperature: r.Unit.Temperature,
		Rssi:        r.Unit.Rssi,
		Samples:     float64(len(cmp1)),
	}

	//ff := fourier.NewCmplxFFT(len(cmp1))
	//cmp1 = ff.Coefficients(nil, cmp1)
	//cmp2 = ff.Coefficients(nil, cmp2)
	//cmp1 = window.BlackmanHarrisComplex(cmp1)
	//cmp2 = window.BlackmanHarrisComplex(cmp2)

	//cmp1 = ff.Sequence(nil, cmp1)
	//cmp1 = ff.Sequence(nil, cmp2)

	//
	//cmp1 = movingAverageFilter(cmp1, 1)
	//cmp2 = movingAverageFilter(cmp2, 1)
	//filter := []complex128{complex(0, 0), complex(1, 1), complex(2, 2), complex(3, 3), complex(0, 0)}
	//////
	////////
	//////w := 24
	//////cmp1 = MedianFilter(cmp1, w)
	//////cmp2 = MedianFilter(cmp2, w)
	//////
	////
	////filter := GenerateFMCWSignal(12.5e-3, 6.10351562e6, 20480)
	//cmp1 = MatchedFilter(cmp1, filter)
	//cmp2 = MatchedFilter(cmp2, filter)
	//cmp1 = movingAverageFilter(cmp1, 1)
	//cmp2 = movingAverageFilter(cmp2, 1)

	//bf := FindBeatFrequency(cmp1, float64(unit.Metadata.Sampling.Frequency))
	//fmt.Println(bf)
	//fmt.Println()
	//ff := fourier.NewCmplxFFT(len(cmp1))
	//unit.Channels[0].Spectrum = []float64{}
	//cmp1 = ff.Coefficients(nil, cmp1)
	//for _, c := range cmp1 {
	//	unit.Channels[0].Spectrum = append(unit.Channels[0].Spectrum, math.Log10(cmplx.Abs(c))*10)
	//}
	//////idx, fr := findPeaks(cmp1, 2)
	//////unit.Channels[0].Peaks = idx
	//////unit.Channels[0].Frequencies = fr
	//cmp2 = ff.Coefficients(nil, cmp2)
	//unit.Channels[1].Spectrum = []float64{}
	//for _, c := range cmp2 {
	//	unit.Channels[1].Spectrum = append(unit.Channels[1].Spectrum, math.Log10(cmplx.Abs(c))*10)
	//}
	cmp1 = removeDCOffset(cmp1)
	cmp2 = removeDCOffset(cmp2)
	//bw := ((float64(r.Unit.Metadata.Chirp.Resolution) / 4096) * 2.5) * 80e6

	//fmt.Println(dst, dst2)
	//cmp1 = dsp.RemoveDCOffset(cmp1, complex(1855, 1855))
	//cmp2 = dsp.RemoveDCOffset(cmp2, complex(1855, 1855))
	//fmt.Println(rangeFromBeatFrequency(cmp1, cmp2, float64(unit.Metadata.Chirp.Duration), 3e8, 1e6))
	bw := ((float64(unit.Metadata.Chirp.Resolution) / 4096) * 2.5) * 80e6
	duration := float64(unit.Metadata.Chirp.Prf)
	if unit.Metadata.Chirp.Padding > 0 {
		duration /= float64(unit.Metadata.Chirp.Steps) / float64(unit.Metadata.Chirp.Padding)
	}

	filter := generateFMCWSawtooth(duration/1000.0/1000.0, float64(unit.Metadata.Sampling.Frequency), bw, 24e9)
	cmp1 = matchedFilterComplex(cmp1, filter)
	cmp2 = matchedFilterComplex(cmp2, filter)
	////cmp1 = filter
	//cmp1 = filter
	//ds := []float64{}
	phaseDiff, _ := phaseDifference(cmp1, cmp2)
	//peakIndices := detectChirpStartTimes(cmp1, 250)
	//fmt.Println(peakIndices)
	//
	//for _, peakIndex := range peakIndices {
	//	//fmt.Printf("Chirp peak at index %d\n", peakIndex)
	//	distance := estimateDistance(cmp1, peakIndex, 20480, 97.656e6, 12.5e-3, 2.99e8)
	//	ds = append(ds, distance)
	//	fmt.Printf("  Estimated distance (sweep bandwidth = %.1e Hz): %.2f meters\n", 1e9, distance)
	//}
	//fmt.Println(ds)
	//aoa := angleOfApproach(phaseDiff, 0.01245606, 8.763)

	degrees := 81.0
	subdegrees := 3.0
	totalDeg := degrees * subdegrees
	bins := make([]float64, int(math.Floor(totalDeg)))
	//ds := make([]float64, int(math.Floor(totalDeg)))

	//
	//r.velocity[0] = append(r.velocity[0], (dst+dst2)/2)
	//r.velocity[1] = append(r.velocity[1], dst2)
	//samples := 250
	//if len(r.velocity[0]) > samples {
	//	r.velocity[0] = r.velocity[0][(len(r.velocity[0]) - samples):]
	//	r.velocity[1] = r.velocity[1][(len(r.velocity[1]) - samples):]
	//}
	for _, f := range phaseDiff {
		if f >= -40 && f <= 40 {
			f *= subdegrees
			idx := int(math.Floor(f) + (totalDeg / 2.0))
			bins[idx]++
			//ds[idx]++
		}

	}
	//
	unit.Phase = movingAverageFilterFloat(bins, 10)
	//unit.Distance = ds

	//c1 := fft.FFT2(r.spec[0])
	//c2 := fft.FFT2(r.spec[1])

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
	//freqs := HighestProbFrequencies(cmp1, 20480, len(cmp1), 40)
	//cmp1 = ff.Coefficients(nil, cmp1)
	//dst := FindBeatFrequency(cmp1, float64(r.Unit.Metadata.Sampling.Frequency))
	//for _, freq := range freqs {
	//	fmt.Println(CalculateVelocity(freq, dst, bw))
	//}
	//

	//idx, fr = findPeaks(cmp2, 2)
	//unit.Channels[1].Peaks = idx
	//unit.Channels[1].Frequencies = fr
	//
	//cmp2 = ff.Coefficients(nil, cmp2)
	//for _, c := range cmp2[0 : len(cmp2)/2] {
	//	unit.Channels[1].Spectrum = append(unit.Channels[1].Spectrum, cmplx.Abs(c))
	//}

	//p

	unit.Metadata.Duration = r.duration
	unit.Metadata.Rate = r.rate
	//
	unit.Channels[0].SignalI = dsp.DownSample(unit.Channels[0].SignalI, 1024)
	unit.Channels[0].SignalQ = dsp.DownSample(unit.Channels[0].SignalQ, 1024)
	//unit.Channels[0].Spectrum = dsp.DownSample(unit.Channels[0].Spectrum, 1024)
	unit.Channels[1].SignalI = dsp.DownSample(unit.Channels[1].SignalI, 1024)
	unit.Channels[1].SignalQ = dsp.DownSample(unit.Channels[1].SignalQ, 1024)
	//unit.Channels[1].Spectrum = dsp.DownSample(unit.Channels[1].Spectrum, 1024)

	r.display.Lock()
	unit.lastChange = false
	r.Unit = unit
	r.display.Unlock()

}
func normalizePhaseDifference(phase float64) float64 {
	for phase >= math.Pi {
		phase -= math.Pi
	}
	for phase < -math.Pi {
		phase += math.Pi
	}
	return phase
}

// FindMaxIndices finds the indices of the highest n values in the input array.
func FindMaxIndices(data []float64, n int) []int {
	type kv struct {
		Index int
		Value float64
	}

	var highestValues []kv
	for i, v := range data {
		if len(highestValues) < n {
			highestValues = append(highestValues, kv{i, v})
			sort.SliceStable(highestValues, func(i, j int) bool {
				return highestValues[i].Value > highestValues[j].Value
			})
		} else if v > highestValues[n-1].Value {
			highestValues[n-1] = kv{i, v}
			sort.SliceStable(highestValues, func(i, j int) bool {
				return highestValues[i].Value > highestValues[j].Value
			})
		}
	}

	indices := make([]int, n)
	for i, v := range highestValues {
		indices[i] = v.Index
	}

	return indices
}

// HighestProbFrequencies takes a complex FFT result and returns an array of the
// highest probability frequencies' indices in that FFT.
func HighestProbFrequencies(fft []complex128, sampleRate, numSamples, n int) []float64 {
	magnitudes := make([]float64, len(fft))

	for i, v := range fft {
		magnitudes[i] = cmplx.Abs(v)
	}

	maxIndices := FindMaxIndices(magnitudes, n)

	frequencies := make([]float64, n)
	for i, index := range maxIndices {
		frequencies[i] = float64(index*sampleRate) / float64(numSamples)
	}

	return frequencies
}

func phaseDifference(cmp1 []complex128, cmp2 []complex128) ([]float64, []float64) {
	var p []float64
	var d []float64

	for i := 0; i < len(cmp1); i++ {
		p1 := cmp1[i]
		p2 := cmp2[i]
		phase1 := cmplx.Phase(p1)
		phase2 := cmplx.Phase(p2)
		//if phase1-phase2 > math.Pi || phase1-phase2 < -math.Pi {
		//	continue
		//}
		// Perform the FFT on the windowed I/Q data

		// Calculate the beat frequency based on the index of the highest peak
		dx := normalizePhaseDifference(phase1 - phase2)
		difference := math.Asin(0.2257 * dx)
		if !math.IsNaN(difference) {
			p = append(p, difference*(180.0/math.Pi))
			//d = append(d, math.Max(0, 0))
		}
	}

	return p, d
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

func generateFMCWSawtooth(duration float64, sampleRate float64, bandwidth float64, initialFrequency float64) []complex128 {
	numSamples := int(duration * sampleRate)
	sawtooth := make([]complex128, numSamples)

	slope := bandwidth / duration

	for i := 0; i < numSamples; i++ {
		tt := float64(i) / sampleRate

		// Generate the FMCW signal with the sawtooth waveform for the frequency modulation
		modulatedFrequency := initialFrequency + (slope*tt-math.Floor(slope*tt))*bandwidth

		// Compute the phase of the FMCW signal at the current time
		phase := 2.0 * math.Pi * modulatedFrequency * tt

		// Generate the complex FMCW signal with cosine and sine functions for real and imaginary parts, respectively
		sawtooth[i] = complex(math.Cos(phase), math.Sin(phase))
	}

	return sawtooth
}
func rms(values []complex128) float64 {
	sum := 0.0
	for _, value := range values {
		sum += math.Pow(cmplx.Abs(value), 2)
	}
	return math.Sqrt(sum / float64(len(values)))
}

func normalize(values []complex128) []complex128 {
	normalized := make([]complex128, len(values))
	rmsValue := rms(values)
	for i, value := range values {
		normalized[i] = value / complex(rmsValue, 0)
	}
	return normalized
}

func matchedFilterComplex(signal, filter []complex128) []complex128 {
	n := len(signal)
	m := len(filter)

	if m > n {
		return nil
	}

	normalizedSignal := normalize(signal)
	normalizedFilter := normalize(filter)

	output := make([]complex128, n-m+1)

	for i := 0; i <= n-m; i++ {
		sum := complex(0, 0)
		for j := 0; j < m; j++ {
			sum += normalizedSignal[i+j] * cmplx.Conj(normalizedFilter[m-1-j])
		}
		output[i] = sum
	}

	return output
}
func lowPassFilter(data []float64, cutoffFreq, sampleRate float64) []float64 {
	RC := 1.0 / (2 * math.Pi * cutoffFreq)
	dt := 1.0 / sampleRate
	alpha := dt / (RC + dt)

	filteredData := make([]float64, len(data))
	filteredData[0] = data[0]

	for i := 1; i < len(data); i++ {
		filteredData[i] = alpha*data[i] + (1-alpha)*filteredData[i-1]
	}

	return filteredData
}
func CalculateVelocity(freqDiff, sweepFreq, sweepBandwidth float64) float64 {
	velocity := (3e8 * freqDiff) / (2 * sweepFreq * sweepBandwidth)
	return velocity
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
	r.duration = float64(adcStop - adcStart)
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

	r.total += 1

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

	//

	r.mutex.Lock()
	r.spec[0] = append(r.spec[0], cmp1)
	numChirps := 8
	if len(r.spec[0]) > numChirps {
		r.spec[0] = r.spec[0][len(r.spec[0])-numChirps:]
	}
	r.spec[1] = append(r.spec[1], cmp2)
	if len(r.spec[1]) > numChirps {
		r.spec[1] = r.spec[1][len(r.spec[1])-numChirps:]
	}
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
func matchedFilter(signal, filter []float64) []float64 {
	n := len(signal)
	m := len(filter)

	if m > n {
		return nil
	}

	output := make([]float64, n-m+1)

	for i := 0; i <= n-m; i++ {
		sum := 0.0
		for j := 0; j < m; j++ {
			sum += signal[i+j] * filter[m-1-j]
		}
		output[i] = sum
	}

	return output
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

func (r *RemoteUnit) updateSettings(w http.ResponseWriter, req *http.Request) {

	var buf bytes.Buffer

	_, err := buf.ReadFrom(req.Body)
	if err != nil {
		fmt.Println(err)
		return
	}

	md := Metadata{}

	client := http.Client{}
	response, err := client.Post("http://192.168.4.1/system", "application/json", &buf)
	if err != nil {
		fmt.Println(err)
		return
	}
	all, err := io.ReadAll(response.Body)
	if err != nil {
		fmt.Println(err)
		return
	}

	md = Metadata{}
	md.Connected = true
	err = json.Unmarshal(all, &md)
	if err != nil {
		fmt.Println(err)
		return
	}

	r.display.Lock()
	r.Unit.Metadata = md
	r.display.Unlock()

	req.Body.Close()
	w.WriteHeader(200)
	_, err = w.Write([]byte("OK"))
	if err != nil {
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

	router := chi.NewRouter()

	router.Use(cors.Handler(cors.Options{
		// AllowedOrigins:   []string{"https://foo.com"}, // Use this to allow specific origin hosts
		AllowedOrigins: []string{"https://*", "http://*"},
		// AllowOriginFunc:  func(r *http.Request, origin string) bool { return true },
		AllowedMethods:   []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Accept", "Authorization", "Content-Type", "X-CSRF-Token"},
		ExposedHeaders:   []string{"Link"},
		AllowCredentials: false,
		MaxAge:           300, // Maximum value not ignored by any of major browsers
	}))
	router.Post("/update", r.updateSettings)
	go func() {
		err := http.ListenAndServe(":5043", router)
		if err != nil {
		}
	}()

	r.threshold = make([]float64, BufferSize*IncomingSize)
	r.buffer[0] = []complex128{}
	r.buffer[1] = []complex128{}
	r.velocity[0] = []float64{}
	r.velocity[1] = []float64{}
	r.velocity[2] = []float64{}
	r.velocity[3] = []float64{}
	r.spec = map[int][][]complex128{}
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
