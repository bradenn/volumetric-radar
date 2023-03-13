import {createRouter, createWebHistory} from "vue-router";
import HomeView from "../views/HomeView.vue";
import Wifi from "@/views/setup/wifi/Wifi.vue";
import SelectSSID from "@/views/setup/wifi/SelectSSID.vue";
import Modules from "@/views/Modules.vue";
import Zones from "@/views/zones/Zones.vue";
import CreateZone from "@/views/zones/CreateZone.vue";
import ZoneView from "@/views/zones/ZoneView.vue";

const router = createRouter({
    history: createWebHistory(import.meta.env.BASE_URL),
    routes: [
        {
            path: "/",
            name: "home",
            component: HomeView,
        },
        {
            path: "/modules",
            name: "modules",
            component: Modules,
        },
        {
            path: "/zones",
            name: "zones",
            component: ZoneView,
            children: [
                {
                    path: "/zones",
                    name: "viewZones",
                    component: Zones,
                },
                {
                    path: "/zones/create",
                    name: "createZone",
                    component: CreateZone,
                }
            ]
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
