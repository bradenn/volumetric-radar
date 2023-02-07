import { createRouter, createWebHistory } from "vue-router";
import HomeView from "../views/HomeView.vue";
import Wifi from "@/views/setup/wifi/Wifi.vue";
import SelectSSID from "@/views/setup/wifi/SelectSSID.vue";
import Spectrum from "@/views/spectrum/Spectrum.vue";

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: "/",
      name: "spectrum",
      component: Spectrum,
    },
    {
      path: "/setup/wifi",
      name: "wifi",
      component: Wifi,
      children: [{
        path: "/setup/wifi",
        name: "ssid",
        component: SelectSSID,
      }]
    },
  ],
});

export default router;
