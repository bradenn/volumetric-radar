package service

import "bridge/internal/domain/model"

type deviceService struct {
	//model.DeviceService
}

func (d *deviceService) Connect(address string) (*model.Device, error) {

	return nil, nil
}

func NewDeviceService() model.DeviceService {
	return &deviceService{}
}
