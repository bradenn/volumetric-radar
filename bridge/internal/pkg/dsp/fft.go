package dsp

import "gonum.org/v1/gonum/dsp/fourier"

func FFT(signal []complex128) []complex128 {
	cFFT := fourier.NewCmplxFFT(len(signal))
	return cFFT.Coefficients(nil, signal)
}

func IFFT(signal []complex128) []complex128 {
	cFFT := fourier.NewCmplxFFT(len(signal))
	return cFFT.Sequence(nil, signal)
}
