package routes

import (
	"bridge/internal/modules"
	"github.com/go-chi/chi"
	"net/http"
)

type ZoneRouter struct {
	modules.ZoneModule
}

func NewZoneRouter(module modules.ZoneModule) ZoneRouter {
	return ZoneRouter{}
}

func (z ZoneRouter) Route(r chi.Router) {
	r.Post("/zones/create", z.create)
	r.Post("/zones/delete", z.delete)
}

func (z ZoneRouter) create(w http.ResponseWriter, r *http.Request) {

}

func (z ZoneRouter) delete(w http.ResponseWriter, r *http.Request) {

}
