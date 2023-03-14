package pkg

import (
	"math/cmplx"
	"strconv"
)

func hexStringToIntArray(hexStr string) []float64 {
	// Determine the length of the int array
	intArrLen := len(hexStr) / 2

	// Allocate space for the int array
	intArr := make([]float64, intArrLen)

	// Convert each pair of hex digits to an int and store it in the array
	for i := 0; i < intArrLen; i++ {
		hexPair := hexStr[i*2 : i*2+2]
		hexInt, err := strconv.ParseInt(hexPair, 16, 64)
		if err != nil {
			return intArr
		}

		intArr[i] = float64(int(hexInt))
	}

	return intArr
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
func normalizeArray(arr []float64) []float64 {
	// Find the minimum and maximum values in the array
	var minVal, maxVal float64
	for _, val := range arr {
		if val < minVal {
			minVal = val
		}
		if val > maxVal {
			maxVal = val
		}
	}

	// Normalize each element in the array
	for i, val := range arr {
		arr[i] = (val - minVal) / (maxVal - minVal)
	}
	return arr
}

func BinToFreq(binIndex, fftSize int, sweepRate, chirpDuration float64) float64 {
	//c := 3e8 // speed of light in meters per second
	rampDuration := chirpDuration / 2.0
	sweepBandwidth := sweepRate * rampDuration
	freqResolution := sweepBandwidth / float64(fftSize)
	return float64(binIndex)*freqResolution - sweepBandwidth/2.0
}

const C = 299792458
const Fs = 250e6

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

func cvt(n []int) []float64 {
	var out []float64
	for _, u := range n {
		out = append(out, float64(uint16(u)))
	}
	return out
}
