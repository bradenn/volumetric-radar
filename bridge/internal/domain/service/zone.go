package service

import (
	"bridge/internal/domain/model"
)

type zoneService struct {
	repo model.ZoneRepository
}

func (z zoneService) Connect(address string) (*model.Device, error) {
	//TODO implement me
	panic("implement me")
}

func NewZoneService(repo model.ZoneRepository) model.ZoneService {
	return &zoneService{
		repo: repo,
	}
}
