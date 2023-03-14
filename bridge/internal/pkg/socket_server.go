package pkg

import (
	"fmt"
	"github.com/google/uuid"
	"net/http"
)

type SocketServer struct {
	connections map[string]*SocketClient
}

func (s *SocketServer) handle(w http.ResponseWriter, r *http.Request) {

	var id string

	for {
		newUUID, err := uuid.NewUUID()
		if err != nil {
			return
		}
		id = newUUID.String()
		if s.connections[id] == nil {
			break
		}
	}

	client, err := NewSocketClient(w, r, id)
	if err != nil {
		return
	}

	client.conn.SetCloseHandler(func(code int, text string) error {
		delete(s.connections, client.Id)
		return nil
	})

	s.connections[id] = client
}

func (s *SocketServer) Broadcast(data []byte) error {
	for _, client := range s.connections {
		if client != nil {
			client.send(data)
		}
	}
	return nil
}

func (s *SocketServer) Serve() {

	http.HandleFunc("/", s.handle)

	err := http.ListenAndServe("0.0.0.0:5500", nil)
	if err != nil {
		fmt.Println("read:", err)
	}

}

func NewSocketServer() (*SocketServer, error) {
	s := SocketServer{
		connections: map[string]*SocketClient{},
	}

	return &s, nil
}
