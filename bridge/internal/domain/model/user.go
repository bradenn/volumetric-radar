package model

import (
	"bridge/internal/adaptors"
)

type User struct {
	adaptors.Mutable
	Username  string `json:"username"`
	Firstname string `json:"firstname"`
	Lastname  string `json:"lastname"`
	Email     string `json:"email"`
	Password  string `json:"password"`
}

type UserRepository interface {
	Crud[User]
}

type UserService interface {
	Register(user *User) (string, error)
	Authenticate(user *User) (string, error)
}
