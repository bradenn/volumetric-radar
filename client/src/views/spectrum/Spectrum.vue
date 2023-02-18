<script setup lang="ts">


import {onMounted, onUnmounted, reactive} from "vue";
import Wave from "@/views/spectrum/Wave.vue";
import Polar from "@/components/Polar.vue";
import FFT from "@/views/spectrum/FFT.vue";


interface Datatype {
  ch0?: number[]
  payload: string
  waiting?: number
}

const state = reactive({
  socket: {} as WebSocket,
  data: {} as Datatype,
  canvas: {} as HTMLCanvasElement,
  ch0: [] as number[],
  ch1: [] as number[],
  ch2: [] as number[],
  last: 0,
  lastUpdate: 0,
  out: "",
  waiting: 0,
  ctx: {} as CanvasRenderingContext2D,
  connected: false,
  frequencies: [] as number[]
})

onMounted(() => {
  connect()
})

onUnmounted(() => {

})


function close() {
  state.socket.close();
}

function connect() {

  let ws = new WebSocket("ws://10.0.1.11:5500/")

  ws.onopen = wsConnect;
  ws.onmessage = wsMessage;
  ws.onclose = wsClose;

  state.socket = ws

}

function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}

interface POS {
  x: number
  y: number
}


function wsConnect(e: Event) {
  // state.socket.send("Ping!")
  state.connected = true
}

function hexStringToUint32(hex: string): number {
  return parseInt(hex, 16);
}

function unpackArray(hexString: string): number[] {
  const packedValues: number[] = [];
  for (let i = 0; i < hexString.length; i += 8) {
    const packedValue = hexStringToUint32(hexString.substr(i, 8));
    const value1 = packedValue & 0xffff;
    const value2 = (packedValue >> 16) & 0xffff;
    packedValues.push(value2, value1);
  }
  return packedValues;
}

function wsMessage(e: MessageEvent) {
  state.data = {} as Datatype
  state.last = Date.now() - state.lastUpdate
  state.lastUpdate = Date.now()
  state.data = JSON.parse(e.data) as Datatype
  state.waiting = 0
  // console.log(state.data)

  // state.out = state.data.payload.length;
  // return

  // let out = unpackArray(state.data.payload);
  let obj = JSON.parse(state.data.payload) as {
    t?: number[]
    r?: number[],
    k?: number[],
    frequencies?: number[],
  }
  state.ch0 = []
  state.ch1 = []
  state.ch2 = []
  state.frequencies = []
  state.ch1 = obj.t || []
  state.ch0 = obj.k || []
  state.ch2 = obj.r || []
  state.frequencies = obj.frequencies || []
  // if (state.data.ch1) {
  //   state.ch1 = [];
  //   state.ch1 = state.data.ch1;
  // }
}

function wsClose(e: Event) {

}

</script>

<template>
  {{ state.last }} - {{ state.waiting }} {{ state.connected }}
  <h2>Channel 0</h2>
  <div class="tool-grid">
    <Polar delta="80" name="Horizontal"></Polar>
    <Polar :landscape="true" delta="34" name="Vertical"></Polar>
  </div>
  <FFT :frequencies="state.frequencies" :lut="state.ch1" :spectrum="state.ch0" name="FFT"></FFT>
  <Wave :fft="false" :values="state.ch2" name="Signal"></Wave>
  <!--  <Polar :r="state.ch0" :t="state.ch1" name="Input" :fft="false"></Polar>-->
</template>

<style scoped>
.tool-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(20rem, 1fr));
  gap: 0.5rem;
}
</style>
