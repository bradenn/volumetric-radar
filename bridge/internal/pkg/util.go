package pkg

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"math"
	"math/cmplx"
	"strconv"
)

func extractInt64FromBytes(data []byte, startIndex, endIndex int) (int64, error) {
	if endIndex >= len(data) || startIndex < 0 || endIndex < startIndex || (endIndex-startIndex+1) != 8 {
		return 0, fmt.Errorf("invalid start and end index")
	}

	buf := bytes.NewBuffer(data[startIndex : endIndex+1])
	var result int64
	err := binary.Read(buf, binary.BigEndian, &result)
	if err != nil {
		return 0, err
	}

	return result, nil
}
func decodeBinaryToFloat64Arrays(binaryData []byte, bufferLen int) [][]float64 {
	// Calculate the total number of uint16 arrays in the binary data
	numArrays := len(binaryData) / (bufferLen * 2)

	// Allocate memory for the array of float64 arrays
	floatArrays := make([][]float64, numArrays)

	// Loop through each uint16 array in the binary data and decode it into a float64 array
	for i := 0; i < numArrays; i++ {
		// Calculate the start position in the binary data for this uint16 array
		start := i * bufferLen * 2

		// Decode the uint16 array into a float64 array
		floatArray := make([]float64, bufferLen)
		for j := 0; j < bufferLen; j++ {
			value := binary.BigEndian.Uint16(binaryData[start+j*2 : start+(j+1)*2])
			floatArray[j] = float64(value)
		}

		// Append the float64 array to the array of float64 arrays
		floatArrays[i] = floatArray
	}

	return floatArrays
}

func hexStringToIntArray(hexStr string) ([]int, error) {
	decoded, err := hex.DecodeString(hexStr)
	if err != nil {
		return nil, err
	}
	intArray := make([]int, len(decoded)/2)
	for i := 0; i < len(decoded)/2; i++ {
		intArray[i] = int(int16(decoded[2*i])<<8 | int16(decoded[2*i+1]))
	}
	return intArray, nil
}

func intArrayToFloatArray(intArray []int) []float64 {
	floatArray := make([]float64, len(intArray))
	for i := 0; i < len(intArray); i++ {
		floatArray[i] = float64(intArray[i]) / float64(math.MaxInt16)
	}
	return floatArray
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
	minVal := 0.0
	maxVal := 0.0
	for _, val := range arr {
		if val < minVal {
			minVal = val
		}
		if val > maxVal {
			maxVal = val
		}
	}
	if maxVal == 0 {
		return arr
	}
	out := make([]float64, len(arr))
	// Normalize each element in the array
	for i, val := range arr {
		out[i] = (val - minVal) / (maxVal - minVal)
	}
	return out
}

func BinToFreq(binIndex, fftSize int, sweepRate, chirpDuration float64) float64 {
	//c := 3e8 // speed of light in meters per second
	rampDuration := chirpDuration / 2.0
	sweepBandwidth := sweepRate * rampDuration
	freqResolution := sweepBandwidth / float64(fftSize)
	return float64(binIndex)*freqResolution - sweepBandwidth/2.0
}

const C = 299792458
const Fs = 24.125e9

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
