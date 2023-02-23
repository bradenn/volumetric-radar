package service

import (
	"bridge/internal/domain/model"
)

type userService struct {
	repo model.UserRepository
}

func (u *userService) Register(user *model.User) (string, error) {
	//TODO implement me
	panic("implement me")
}

func (u *userService) Authenticate(user *model.User) (string, error) {
	//TODO implement me
	panic("implement me")
}

func NewUserService(repo model.UserRepository) model.UserService {
	return &userService{
		repo: repo,
	}
}
