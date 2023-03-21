<script setup lang="ts">


import {onBeforeUnmount, onMounted, reactive} from "vue";
import UnitDom from "@/views/spectrum/Unit.vue";
import type {Channel, Unit} from "@/types";
import {state} from "vue-tsc/out/shared";
import Header from "@/components/Header.vue";
import Tag from "@/components/Tag.vue";

// interface Unit {
//   channel: Channel
//   metadata: {
//     base: number // 80000Hz
//     samples: number   // 8Hz
//     frequency: number   // 10000Hz
//     chirp: number // 1000Hz (once per ms)
//   }
//   phase: number[]
// }
//
// interface Channel {
//   signalI: number[]
//   signalQ: number[],
//   spectrum: number[],
//   frequencies: number[],
//   peaks: number[],
// }

interface Metadata {
  frame: number[]
}

const state = reactive({
  socket: {} as WebSocket,
  units: [] as Unit[],
  connectionStatus: "",
  reconnect: 0,
  data: [{
    signalI: [],
    signalQ: [],
    spectrum: [],
    frequencies: [],
    peaks: []
  }, {
    signalI: [],
    signalQ: [],
    spectrum: [],
    frequencies: [],
    peaks: []
  }] as Channel[],
  canvas: {} as HTMLCanvasElement,
  metadata: {

    last: 0,
    since: 0,
    count: 0,
    updates: 0
  },
  start: 0,
  ctx: {} as CanvasRenderingContext2D,
  connected: false,
})

onMounted(() => {
  connect()
})

onBeforeUnmount(() => {
  state.units = []
  close()
})


function close() {
  state.socket.close(1000, "")
}

function connect() {
  let ws = new WebSocket("ws://localhost:5500/")


  ws.onopen = wsConnect;
  ws.onmessage = wsMessage;
  ws.onclose = wsClose;
  state.metadata.last = Date.now()
  state.socket = ws

}

function wsConnect(e: Event) {
  state.socket.send("Ping!")
  state.start = new Date().valueOf()
  clearTimeout(state.reconnect)
  state.reconnect = 0
  state.connected = true
}


function wsMessage(e: MessageEvent) {

  let b = JSON.parse(e.data) as Unit
  if (!b) {
    state.units = [{},{}] as Unit[]
    return
  }
  if(state.units.filter(u => u.metadata.mac === b.metadata.mac).length > 0) {
    state.units = state.units.map(u => ((u.metadata.mac === b.metadata.mac) ? b : u))
  }else{
    state.units.push(b)
  }
  let now = Date.now()
  state.metadata.since = now - state.metadata.last
  state.metadata.count++
  state.metadata.updates = Math.round(state.metadata.count / (state.metadata.since / 1000) * 100) / 100

  if (state.metadata.since > 1000) {
    state.metadata.last = now
    state.metadata.count = 0
  }
}

function wsClose(e: Event) {
  state.connected = false
  state.reconnect = setTimeout(connect, 1000)
}

</script>

<template>

  <div class="d-flex flex-column gap-2 mt-3">

    <Header name="Dashboard">
      <Tag :active="state.connected?'label-success':'label-warning'" :value="state.connected?'Connected':'Disconnected'"
           name="Bridge"></Tag>
      <Tag :disabled="!state.connected" :value="`${state.metadata.updates.toFixed(2)} up/s`" name="Updates"></Tag>
    </Header>

  </div>

  <div class="d-flex flex-column gap-1" v-if="state.connected">
    <div v-if="state.units.length > 0" class="d-flex flex-column gap-1 mt-2">
      <UnitDom v-for="unit in state.units" :key="unit.metadata.mac" :name="unit.metadata.name" :unit="unit"></UnitDom>
    </div>
  </div>
</template>

<style scoped>
.demo-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(20rem, 1fr));
  gap: 0.5rem;
}

.tool-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(20rem, 1fr));
  gap: 0.5rem;
}
</style>
