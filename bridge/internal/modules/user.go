package modules

import (
	"bridge/internal/adaptors"
	"bridge/internal/domain/model"
	"bridge/internal/domain/repository"
	"bridge/internal/domain/service"
)

type UserModule struct {
	model.UserService
}

func NewUserModule(db *adaptors.Database) UserModule {
	repo := repository.NewUserRepository(db)
	return UserModule{
		UserService: service.NewUserService(repo),
	}
}
