package socket

import (
	"bridge/internal/infrastructure/log"
	"net"
)

type Socket struct {
	conn net.Conn
}

func NewSocket(address string) (*Socket, error) {
	conn, err := net.Dial("tcp", address)
	if err != nil {
		return nil, err
	}

	socket := &Socket{
		conn: conn,
	}

	go socket.run()

	return socket, nil

}

func (s *Socket) run() {
	defer func(conn net.Conn) {
		err := conn.Close()
		if err != nil {
			log.Err(err)
		}
	}(s.conn)
	_, err := s.conn.Write([]byte("Jello"))
	if err != nil {
		log.Event("Write failed!")
		return
	}
	log.Event("Listening!")
	for {
		buffer := make([]byte, 1024)
		n, err := s.conn.Read(buffer)
		if err != nil {
			log.Event("Read failed!")
			return
		}

		log.Event("%s", buffer[0:n])

	}
}
