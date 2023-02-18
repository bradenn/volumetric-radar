package pkg

import (
	"encoding/json"
	"fmt"
	"github.com/eripe970/go-dsp-utils"
	"github.com/gorilla/websocket"
	"log"
	"net/http"
	"strconv"
	"time"
)

type Remote struct {
	timeStarted time.Time
	conn        *websocket.Conn
	out         *websocket.Conn
	buffer      []uint16
	dsp         map[string][]float64
	b           chan []uint16
	stream      chan []byte
}

const (
	frameWidth = 2 << 9
)

func findLocalMaxima(signal []float64, threshold float64) []int {
	var localMaxima []int

	for i := 1; i < len(signal)-1; i++ {
		if signal[i] > signal[i-1] && signal[i] > signal[i+1] && signal[i] >= threshold {
			localMaxima = append(localMaxima, i)
		}
	}

	return localMaxima
}

func (s *Remote) serve() {

	http.HandleFunc("/", s.runWs)

	err := http.ListenAndServe("0.0.0.0:5500", nil)
	if err != nil {
		log.Println("read:", err)
	}

}

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

func (s *Remote) runWs(w http.ResponseWriter, r *http.Request) {
	var err error
	s.out, err = upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println("read:", err)
		return
	}
	defer s.out.Close()
	for {
		_, _, err = s.out.ReadMessage()
		if err != nil {
			log.Println("read:", err)
			continue
		}
	}
}

func (s *Remote) output() {
	p := Packet{}
	var err error
	out, err := json.Marshal(s.dsp)
	if err != nil {
		log.Println("dsp arr:", err)
	}
	p.Payload = string(out)
	out, err = json.Marshal(p)
	if err != nil {
		log.Println("read:", err)
	}
	//fmt.Println(string(out))
	s.stream <- out
}

func lerp(a, b, t float64) float64 {
	return a*(1-t) + b*t
}

func (s *Remote) process() {
	r := make([]float64, 0)
	for i := range s.buffer {
		r = append(r, float64(s.buffer[i]))
	}

	signal := dsp.Signal{
		SampleRate: 6488,
		Signal:     r,
	}

	normalized, err := signal.Normalize()
	if err != nil {
		return
	}

	spectrum, err := normalized.FrequencySpectrum()
	if err != nil {
		return
	}

	peakLocations := findLocalMaxima(spectrum.Spectrum, spectrum.Max()/4)
	var peaks []float64
	for i := range peakLocations {
		peaks = append(peaks, float64(peakLocations[i]))
	}
	s.dsp["frequencies"] = peaks
	s.dsp["r"] = normalized.Signal
	s.dsp["t"] = spectrum.Frequencies
	s.dsp["k"] = spectrum.Spectrum
	s.output()
}

func (s *Remote) run() {
	for income := range s.b {

		s.buffer = append(s.buffer, income...)
		if len(s.buffer) > frameWidth {
			s.buffer = s.buffer[len(s.buffer)-frameWidth : len(s.buffer)]
			//fmt.Println("Append len : ", len(s.buffer))
			s.process()
		}
		//if len(s.buffer) < frameWidth {
		//	for i := 0; i < frameWidth; i++ {
		//		s.buffer = append(s.buffer, uint16(2048+math.Sin(float64(i)*((math.Pi*2.0)/frameWidth)*10+(rand.NormFloat64()/50))*2048))
		//	}
		//}
		//s.process()

	}
}

func (s *Remote) srv() {
	for income := range s.stream {
		if s.out != nil {
			//fmt.Println(income)
			_ = s.out.WriteMessage(websocket.TextMessage, income)
		}
	}
}

type Runnable struct {
}

type Packet struct {
	Payload string `json:"payload"`
	Ch0     []float64
}

func unpackArray(hexString string) []uint16 {
	packedValues := make([]uint16, 0)
	for i := 0; i < len(hexString); i += 8 {
		packedValue, _ := strconv.ParseUint(hexString[i:i+8], 16, 32)
		value1 := uint16(packedValue & 0xffff)
		value2 := uint16((packedValue >> 16) & 0xffff)
		packedValues = append(packedValues, value2, value1)
	}

	return packedValues
}

func NewServer(host string) *Remote {

	srv := &Remote{
		dsp: map[string][]float64{},
	}
	var err error
	srv.conn, _, err = websocket.DefaultDialer.Dial(host, nil)
	if err != nil {
		log.Println("read:", err)
		return nil
	}
	err = srv.conn.WriteMessage(websocket.TextMessage, []byte("Ping!"))
	if err != nil {
		return nil
	}

	srv.begin()

	return srv

}

func (s *Remote) begin() {

	go func() {
		s.serve()
	}()
	go func() {
		s.run()
	}()
	go func() {
		s.srv()
	}()
	s.b = make(chan []uint16, 1)
	s.stream = make(chan []byte, 1)
	//lastCheck := time.Now()
	for {
		_, message, err := s.conn.ReadMessage()
		if err != nil {
			log.Println("read:", err)
			continue
		}
		p := Packet{}
		err = json.Unmarshal(message, &p)
		if err != nil {
			fmt.Printf("Error: %s\n", err)
			continue
		}

		//fmt.Println(p.Payload)
		s.b <- unpackArray(p.Payload)
	}
}
