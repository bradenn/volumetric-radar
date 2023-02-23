package adaptors

import (
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"time"
)

type Mutable struct {
	Id      string    `json:"id"`
	Created time.Time `json:"created"`
	Updated time.Time `json:"updated"`
}

func (m *Mutable) Update() {
	m.Updated = time.Now()
}

func (m *Mutable) InitId(id string) {
	m.Id = id
	m.Created = time.Now()
	m.Updated = time.Now()
}

type Database struct {
	client *gorm.DB
}

func NewDatabase() (*Database, error) {
	db, err := gorm.Open(sqlite.Open("test.db"), &gorm.Config{})
	if err != nil {
		panic("failed to connect database")
	}

	return &Database{client: db}, nil
}
