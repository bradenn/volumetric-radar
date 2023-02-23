package modules

import (
	"bridge/internal/adaptors"
	"bridge/internal/domain/model"
	"bridge/internal/domain/repository"
	"bridge/internal/domain/service"
)

type DeviceModule struct {
	model.DeviceService
}

func NewDeviceModule(db *adaptors.Database) DeviceModule {
	return DeviceModule{
		DeviceService: service.NewDeviceService(repository.NewDeviceRepository(db)),
	}
}
