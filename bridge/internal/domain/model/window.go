package model

import (
	"net"
	"strconv"
	"time"
)

type Window struct {
	Source net.HardwareAddr `json:"source"`
	Begin  time.Time        `json:"begin"`
	buffer []float64
}

func unpackArray(hexString string) []uint16 {
	packedValues := make([]uint16, 0)
	for i := 0; i < len(hexString); i += 8 {
		packedValue, _ := strconv.ParseUint(hexString[i:i+8], 16, 32)
		value1 := uint16(packedValue & 0xffff)
		value2 := uint16((packedValue >> 16) & 0xffff)
		packedValues = append(packedValues, value2, value1)
	}

	return packedValues
}

func (w *Window) FromHex(source net.HardwareAddr, begin time.Time, hex string) error {
	data := unpackArray(hex)
	w.Source = source
	w.Begin = begin
	w.buffer = make([]float64, len(data))
	for i, datum := range data {
		w.buffer[i] = float64(datum)
	}

	return nil
}

type WindowRepository interface {
}

type WindowService interface {
}
