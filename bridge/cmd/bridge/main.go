package main

import (
	"bridge/internal/core"
)

func main() {
	//_, err := socket.NewSocket("10.0.1.85:4567")
	//if err != nil {
	//	log.Log("It seems to be not working: %s", err)
	//	return
	//}
	core.NewController()
	//_ = pkg.NewServer("ws://10.0.1.85/ws")
}
