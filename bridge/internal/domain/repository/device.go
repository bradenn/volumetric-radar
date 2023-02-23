package repository

import (
	"bridge/internal/adaptors"
	"bridge/internal/domain/model"
)

type deviceRepository struct {
	adaptors.Store[model.Device]
}

func NewDeviceRepository(store *adaptors.Database) model.DeviceRepository {
	return &deviceRepository{
		Store: *adaptors.NewRedisStore[model.Device](store),
	}
}
