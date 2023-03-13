package modules

import (
	"bridge/internal/adaptors"
	"bridge/internal/domain/model"
	"bridge/internal/domain/repository"
	"bridge/internal/domain/service"
)

type ZoneModule struct {
	model.ZoneService
}

func NewZoneModule(db *adaptors.Database) ZoneModule {
	repo := repository.NewUserRepository(db)
	return ZoneModule{
		ZoneService: service.NewZoneService(repo),
	}
}
