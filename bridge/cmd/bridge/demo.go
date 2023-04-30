package main

import (
	"bridge/internal/pkg"
	"fmt"
	"github.com/go-chi/chi"
	"github.com/gorilla/websocket"
	"math/rand"
	"net/http"
	"time"
)

func handleWS(w http.ResponseWriter, r *http.Request) {
	ug := websocket.Upgrader{
		HandshakeTimeout:  1000,
		ReadBufferSize:    2048,
		WriteBufferSize:   2048,
		WriteBufferPool:   nil,
		Subprotocols:      nil,
		Error:             nil,
		EnableCompression: false,
	}
	conn, err := ug.Upgrade(w, r, nil)
	if err != nil {
		return
	}

	ud := pkg.UnitData{
		Name: "Demo Unit",
		Mac:  "na",
		Base: 24e9,
		XFov: 80,
		YFov: 34,
		Adc: struct {
			Chirp     pkg.Chirp `json:"chirp"`
			Base      float64   `json:"base"`
			Frequency float64   `json:"frequency"`
			Samples   int       `json:"samples"`
			Prf       int       `json:"prf"`
			Window    int       `json:"window"`
			Bits      int       `json:"bits"`
			Pulse     int       `json:"pulse"`
		}{
			Chirp: pkg.Chirp{
				PRF:        10,
				Duration:   10,
				Steps:      10,
				Padding:    10,
				Resolution: 1024,
			},
			Window: 256,
		},
	}

	go func() {
		data := make([]byte, 256*2*4+16)
		for i := range data {
			data[i] = byte(rand.Int())
		}
		err = conn.WriteJSON(ud)
		if err != nil {
			fmt.Println(err)
		}
		_, _, _ = conn.ReadMessage()
		for {
			err = conn.WriteMessage(websocket.BinaryMessage, data)
			if err != nil {
				fmt.Println(err)
			}
			time.Sleep(time.Microsecond * 250)
		}
	}()
}

func main() {
	r := chi.NewRouter()

	r.Get("/ws", handleWS)

	err := http.ListenAndServe("0.0.0.0:4043", r)
	if err != nil {
		return
	}

}
