package model

import (
	"bridge/internal/adaptors"
	"github.com/gorilla/websocket"
	"net"
	"time"
)

type Device struct {
	adaptors.Mutable
	Name      string           `json:"name"`
	Firmware  string           `json:"firmware"`
	Address   string           `json:"address"`
	Mac       net.HardwareAddr `json:"mac"`
	LastSeen  time.Time        `json:"lastSeen"`
	Connected bool             `json:"connected"`
	Frequency int              `json:"frequency"`
	Samples   int              `json:"samples"`
	HFov      float64          `json:"hFov"`
	VFov      float64          `json:"vFov"`
	conn      *websocket.Conn
}

type Crud[T any] interface {
	Create(val *T) error
	Delete(val *T) error
	Update(val *T) error
}

type DeviceRepository interface {
	Crud[Device]
}

type DeviceService interface {
	Connect(address string) (*Device, error)
	Create(name string, address string) (*Device, error)
}
