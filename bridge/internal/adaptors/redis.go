package adaptors

import (
	"gorm.io/gorm"
)

type IMutable interface {
	InitId(id string)
	Update()
}

type Store[T any] struct {
	db *gorm.DB
}

func NewRedisStore[T any](db *Database) *Store[T] {
	return &Store[T]{db: db.client}
}

func (d *Store[T]) Create(t *T) error {
	if err := d.db.Create(t).Error; err != nil {
		return err
	}
	return nil
}

func (d *Store[T]) Delete(t *T) error {
	if err := d.db.Delete(t).Error; err != nil {
		return err
	}
	return nil
}

func (d *Store[T]) Update(t *T) error {
	if err := d.db.Save(t).Error; err != nil {
		return err
	}
	return nil
}
