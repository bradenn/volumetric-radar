<script setup lang="ts">


import {onBeforeUnmount, onMounted, reactive} from "vue";
import UnitDom from "@/views/spectrum/Unit.vue";
import type {Channel, Unit} from "@/types";
import {state} from "vue-tsc/out/shared";

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
  state.reconnect = 1000
  setTimeout(connect, state.reconnect)
}

</script>

<template>
  <div class="d-flex flex-column gap-2 mt-3">
    <div class="element p-2">
      <div class="d-flex gap-1 align-items-center gap-1">
        <div class="label-w500 labe-c3 lh-1 px-2">Dashboard</div>
        <div class="px-2"></div>
        <div class="d-flex gap-2 tag label">
          <div v-if="state.connected" class="text-success text-center" style="width: 8rem">Connected</div>
          <div v-else-if="state.reconnect != 0" class="text-warning text-center" style="width: 8rem">Reconnecting...</div>
          <div v-else class="text-warning text-center" style="width: 8rem">Disconnected</div>
        </div>
        <div class="d-flex gap-2 tag label">
          <div class="text-center" style="width: 6rem">{{ state.metadata.updates.toFixed(2) }} up/s</div>
        </div>
        <div class="d-flex gap-2 tag label">
          <div class="text-center" style="width: 6rem">{{ state.data.length }} units</div>
        </div>
      </div>

    </div>
  </div>

  <div class="d-flex flex-column gap-1" v-if="state.connected">
  <div class="d-flex flex-column gap-1 mt-2" v-if="state.units.length > 0">
    <UnitDom v-for="unit in state.units"  :key="unit.metadata.mac" name="Module 1" :unit="unit"></UnitDom>
  </div>
    <!--    <div v-for="d in state.data" class="">-->
    <!--      <div class="d-flex flex-column gap-2">-->
    <!--        <div class=""></div>-->

    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;-->
    <!--        <Scanner v-if="d.signalQ" :name="`RX ${state.data.indexOf(d) }`" :channel="d.signalQ"></Scanner>&ndash;&gt;-->
    <!--        &lt;!&ndash;       &ndash;&gt;-->
    <!--        <div class="d-flex gap-2">-->
    <!--          &lt;!&ndash;          <OverWave :values0="d.signalI" :values1="d.signalQ" :name="`RX ${state.data.indexOf(d) }`"></OverWave>&ndash;&gt;-->
    <!--          <Doppler v-if="d.signalI" :values0="d.spectrum" :values1="d.signalI" name=""></Doppler>-->
    <!--          &lt;!&ndash;          <Doppler  v-if="d.spectrum" :values0="d.spectrum" :values1="d.spectrum" name=""></Doppler>&ndash;&gt;-->
    <!--          &lt;!&ndash;&lt;!&ndash;         <&ndash;&gt;&ndash;&gt;-->
    <!--          <Polar name="dd" :delta="80" :pings="d.phase" style="width: 100%"></Polar>-->
    <!--          &lt;!&ndash;          &ndash;&gt;-->
    <!--        </div>-->
    <!--        &lt;!&ndash;      <FFT :spectrum="d.spectrum" name="dd" :lut="d.frequencies" :frequencies="d.peaks"></FFT>&ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        <Scanner v-if="d.spectrum.length > 0" :name="`RX ${state.data.indexOf(d) }`" :channel="d.spectrum"></Scanner>&ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--        &lt;!&ndash;        &ndash;&gt;-->
    <!--      </div>-->
    <!--      <Wave :fft="false" :values="state.data[0].phase" name="Q"></Wave>-->
    <!--    </div>-->
  </div>


  <!--  <Scanner v-if="state.data[0].frequencies" :name="`RX ${state.data.indexOf(d) }`" :channel="state.data[0].frequencies"></Scanner>-->
  <div class="pt-2" v-if="true">
    <!--    -->
  </div>
  <!--  <Wave :fft="false" :values="state.ch1" name="Q"></Wave>-->
  <!--  -->

  <!--  <Wave :fft="false" :values="state.fft0" name="I"></Wave>-->
  <!--  <Wave :fft="false" :values="state.fft1" name="Q"></Wave>-->
  <!--  -->
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
