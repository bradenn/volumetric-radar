<script setup lang="ts">


import {onMounted, onUnmounted, reactive, watchEffect} from "vue";


interface Datatype {
  values: number[]
}

const state = reactive({
  socket: {} as WebSocket,
  data: [] as Datatype[],
  canvas: {} as HTMLCanvasElement,
  ctx: {} as CanvasRenderingContext2D
})

onMounted(() => {
  connect()

  configureCanvas()
  draw();
})

onUnmounted(() => {

})

watchEffect(() => {
 draw()
  return state.data
})

function close() {
  state.socket.close();
}

function connect() {

  let ws = new WebSocket("ws://10.0.1.85/ws")

  ws.onopen = wsConnect;
  ws.onmessage = wsMessage;
  ws.onclose = wsClose;

  state.socket = ws

}

function configureCanvas() {
  const _canvas = document.getElementById(`signal`)
  state.canvas = _canvas as HTMLCanvasElement
  state.ctx = state.canvas.getContext("2d") as CanvasRenderingContext2D
  let scale = 1
  state.ctx.scale(scale, scale)

  state.canvas.width = state.canvas.clientWidth * scale
  state.canvas.height = state.canvas.clientHeight * scale

  draw()
}

function draw() {
  let ctx = state.ctx;
  if (!ctx.canvas) return
  ctx.lineWidth = 2
  ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
  // drawLegend()
  let w = ctx.canvas.width;
  let h = ctx.canvas.height;
  ctx.strokeStyle = "rgba(255,255,255,0.125)";
  ctx.beginPath()
  ctx.moveTo(0, h / 1.25)
  ctx.lineTo(w, h / 1.25)
  ctx.stroke()
  ctx.closePath()

  let scale = 25;

  for (let i = 0; i < w / scale; i++) {
    ctx.beginPath()
    ctx.moveTo(i * scale, h / 1.25 - 2)
    ctx.lineTo(i * scale, h / 1.25 + 2)
    ctx.stroke()
    ctx.closePath()
  }

  ctx.beginPath()
  for (let i = 0; i < state.data.length; i++) {
    drawPattern(ctx, state.data[i].values, i)
  }
  ctx.closePath()
  ctx.stroke()

}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}

interface POS {
  x: number
  y: number
}

function drawPattern(ctx: CanvasRenderingContext2D, values: number[], depth: number) {


  let minY = Math.min(...values);
  let maxY = Math.max(...values) + 1;
  ctx.fillStyle = "rgba(255,255,255,0.25)";
  ctx.lineWidth = 1
  ctx.strokeStyle = depth == 0?`rgba(255, 128, 0, 0.5)`:`rgba(0, 128, 255, 0.5)`;
  let w = ctx.canvas.width;
  let h = ctx.canvas.height;


  // ctx.moveTo(0, h / 1.25 - (values[0] - minY) / (maxY - minY) * (ctx.canvas.height) / 1.5)
  let lastX = 0;
  let lastY = h / 1.25 - (values[0] - minY) / (maxY - minY) * (ctx.canvas.height) / 1.5;
  let mass = w / (values.length)
  for (let i = 0; i < values.length; i++) {
    let x = i * mass;
    let y = h / 1.25 - (values[(values.length - 1) - i] - minY) / (maxY - minY) * (ctx.canvas.height) / 1.5
    ctx.moveTo(lastX, lastY)
    ctx.lineTo(x, y)
    lastX = x;
    lastY = y
  }


}

function wsConnect(e: Event) {
  state.socket.send("Ping!")
}

function wsMessage(e: MessageEvent) {
  state.data = JSON.parse(e.data) as Datatype[]
}

function wsClose(e: Event) {

}

</script>

<template>
  <h1>Socket Data:</h1>
  <div class="canvas-container ">

    <canvas :id="`signal`" class="inner-canvas"></canvas>
  </div>
</template>

<style>
.inner-canvas {
  width: 100%;
  height: 100%;

}

.canvas-container {
  display: flex;
  flex-direction: row;
  justify-content: center;
  width: 100%;
  height: 12rem;
  align-items: center;
  border-radius: 8px;
  background-color: hsla(214, 9%, 28%, 0.2);
  padding: 6px
}
</style>
