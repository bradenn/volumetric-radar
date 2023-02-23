package service

import (
	"bridge/internal/domain/model"
	"time"
)

type deviceService struct {
	repo model.DeviceRepository
}

func (d *deviceService) Create(name string, address string) (*model.Device, error) {
	err := d.repo.Create(&model.Device{
		Name:      name,
		Address:   address,
		Mac:       nil,
		LastSeen:  time.Time{},
		Connected: false,
		Frequency: 0,
		Samples:   0,
		HFov:      0,
		VFov:      0,
	})
	if err != nil {
		return nil, err
	}
	return nil, nil
}

func (d *deviceService) Connect(address string) (*model.Device, error) {

	return nil, nil
}

func NewDeviceService(repo model.DeviceRepository) model.DeviceService {
	return &deviceService{
		repo: repo,
	}
}
