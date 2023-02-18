import {createApp} from "vue";
import App from "./App.vue";
import router from "./router";
import '@/assets/bootstrap-grid.css';
import '@/assets/bootstrap-utilities.css';
import "./assets/style.css"

const app = createApp(App);

app.use(router);

app.mount("#app");
