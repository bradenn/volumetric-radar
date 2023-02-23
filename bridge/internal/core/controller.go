package core

import (
	"bridge/internal/adaptors"
	"bridge/internal/modules"
	"fmt"
)

type Controller struct {
	Devices modules.DeviceModule
	Users   modules.UserModule
}

func NewController() {

	database, err := adaptors.NewDatabase()
	if err != nil {
		fmt.Println("DB setup failed")
		return
	}
	_ = Controller{
		Devices: modules.NewDeviceModule(database),
		Users:   modules.NewUserModule(database),
	}

}
