package model

type Crud[T any] interface {
	Create(val *T) error
	Delete(val *T) error
	Update(val *T) error
}
