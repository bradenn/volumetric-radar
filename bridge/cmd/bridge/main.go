package main

import (
	"bridge/internal/infrastructure/log"
	"bridge/internal/pkg"
)

func main() {
	server, err := pkg.NewSocketServer()
	if err != nil {
		return
	}

	go server.Serve()

	out := make(chan []byte, 128)
	go func() {
		for bytes := range out {
			err = server.Broadcast(bytes)
			if err != nil {
				log.Err(err)
				continue
			}
		}
	}()

	us, err := pkg.NewUnitServer(out)
	if err != nil {
		return
	}

	us.AddUnit("ws://192.168.4.1/ws")
	//us.AddUnit("ws://localhost:4043/ws")

	for {
	}
}
