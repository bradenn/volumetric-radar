package repository

import (
	"bridge/internal/adaptors"
	"bridge/internal/domain/model"
)

type userRepository struct {
	adaptors.Store[model.User]
}

func NewUserRepository(store *adaptors.Database) model.UserRepository {
	return &userRepository{
		Store: *adaptors.NewRedisStore[model.User](store),
	}
}
