package pkg

import (
	"bridge/internal/infrastructure/log"
	"github.com/gorilla/websocket"
	"net/http"
)

type SocketClient struct {
	conn     *websocket.Conn
	outgoing chan []byte
	done     chan bool
	Id       string
}

func (s *SocketClient) listen() {
	for {
		_, _, err := s.conn.ReadMessage()
		if err != nil {
			s.done <- true
			return
		}
	}
}

func (s *SocketClient) run() {
	log.Event("Client '%s' connected", s.Id)
	defer log.Event("Client '%s' disconnected", s.Id)
	for {
		select {
		case buf := <-s.outgoing:
			s.send(buf)
		case <-s.done:
			return
		}
	}
}

func (s *SocketClient) close() error {
	err := s.conn.Close()
	if err != nil {
		return err
	}
	return nil
}

func (s *SocketClient) send(data []byte) {
	err := s.conn.WriteMessage(websocket.TextMessage, data)
	if err != nil {
		return
	}
}

func NewSocketClient(w http.ResponseWriter, r *http.Request, id string) (*SocketClient, error) {
	var err error
	s := &SocketClient{
		done:     make(chan bool, 5),
		outgoing: make(chan []byte, 4),
		Id:       id,
	}
	s.conn, err = upgrader.Upgrade(w, r, nil)
	if err != nil {
		return nil, err
	}
	//s.conn.SetCloseHandler(func(code int, text string) error {
	//	s.done <- true
	//	return nil
	//})

	go s.run()

	go s.listen()

	return s, nil
}
