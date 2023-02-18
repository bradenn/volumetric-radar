package model

type Zone struct {
	Name string `json:"name"`
}

type ZoneRepository interface {
}

type ZoneService interface {
	Connect(address string) (*Device, error)
}
