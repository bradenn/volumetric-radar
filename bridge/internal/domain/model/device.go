package model

import (
	"github.com/gorilla/websocket"
	"net"
	"time"
)

type Device struct {
	Name      string           `json:"name"`
	Firmware  string           `json:"firmware"`
	Address   net.IPAddr       `json:"address"`
	Mac       net.HardwareAddr `json:"mac"`
	LastSeen  time.Time        `json:"lastSeen"`
	Connected bool             `json:"connected"`
	Frequency int              `json:"frequency"`
	Samples   int              `json:"samples"`
	conn      *websocket.Conn
}

type DeviceRepository interface {
}

type DeviceService interface {
	Connect(address string) (*Device, error)
}
