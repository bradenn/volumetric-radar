package dsp

import "math"

func RemoveDCOffset(data []complex128, offset complex128) []complex128 {
	correctedData := make([]complex128, len(data))

	for i, v := range data {
		correctedData[i] = v - offset
	}

	return correctedData
}

func DownSample(input []float64, n int) []float64 {
	if len(input) <= n {
		// nothing to downSample, return the input as is
		return input
	}

	output := make([]float64, n)

	// compute the downSample factor
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
