package socket

import (
	"bridge/internal/infrastructure/log"
	"github.com/go-chi/chi"
	"net/http"
)

type Server struct {
	chi.Router
	address string
	done    chan bool
}

func (s *Server) listenAndServe() {
	srv := http.Server{}
	srv.Handler = s.Router
	// Create a thread to wait for the done signal
	go func() {
		select {
		case _ = <-s.done:
			err := srv.Close()
			if err != nil {
				return
			}
		}
	}()
	// Continuously restart the server for as long as the server does not close
	for {
		err := srv.ListenAndServe()
		if err != nil {
			if err == http.ErrServerClosed {
				log.Event("HTTP server shutting down...")
				break
			}
			log.Err(err)
		}
	}
}

func NewServer(addr string) *Server {
	srv := Server{
		Router:  chi.NewRouter(),
		address: addr,
		done:    make(chan bool),
	}

	return &srv
}
