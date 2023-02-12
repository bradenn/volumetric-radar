<script setup lang="ts">


import {onMounted, onUnmounted, reactive, watchEffect} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
  name: string
  values: number[]
  fft: boolean
}

let props = defineProps<Datatype>();

const state = reactive({
  lastFew: [] as number[][],
  canvas: {} as HTMLCanvasElement,
  ctx: {} as CanvasRenderingContext2D,
  min: 0,
  max: 0,
  maxIndex: 0,
  top: [] as number[],
  uuid: uuidv4(),
})

onMounted(() => {
  configureCanvas()
  draw();
})

onUnmounted(() => {

})

watchEffect(() => {
  draw()
  return props.values
})

function configureCanvas() {
  const _canvas = document.getElementById(`signal-${state.uuid}`)
  state.canvas = _canvas as HTMLCanvasElement
  state.ctx = state.canvas.getContext("2d") as CanvasRenderingContext2D
  let scale = 2
  state.ctx.scale(scale, scale)

  state.canvas.width = state.canvas.clientWidth * scale
  state.canvas.height = state.canvas.clientHeight * scale

  draw()
}

function matchedFilter(signal: number[], filter: number[]): number[] {
  const signalLength = signal.length;
  const filterLength = filter.length;
  const resultLength = 2 * (signalLength + filterLength - 1);

  const paddedSignal = new Array(resultLength).fill(0);
  const paddedFilter = new Array(resultLength).fill(0);

  for (let i = 0; i < signalLength; i++) {
    paddedSignal[i + filterLength - 1] = signal[i];
  }

  for (let i = 0; i < filterLength; i++) {
    paddedFilter[i + filterLength - 1] = filter[i];
  }

  const result = new Array(resultLength).fill(0);
  for (let i = 0; i < resultLength; i++) {
    for (let j = 0; j <= i; j++) {
      result[i] += paddedSignal[j] * paddedFilter[i - j];
    }
  }

  return result.slice(filterLength - 1, filterLength - 1 + signalLength);
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
  drawPattern(ctx, props.values, 0, false)
  let mapAvg = new Map<number, number>()
  let depth = 0
  for (const hKey in state.lastFew) {
    depth++
    let arr = matchedFilter(state.lastFew[hKey], [0.25, 0.5, 0.25]);
    drawPattern(ctx, arr, 1, false)
    for (let i = 4; i < arr.length; i++) {
      mapAvg.set(i, arr[i] + (mapAvg.get(i) || 0));
    }
  }
  ctx.closePath()
  ctx.stroke()

  if (props.fft) {
    let ou = [] as number[]
    mapAvg.forEach(a => ou.push(a))
    let m = Math.max(...ou)

    let arr = matchedFilter(ou.slice(1), [0.25, 0.5, 0.25]);
    ctx.beginPath()
    drawPattern(ctx, arr.map(v => (v)), -1, false)
    ctx.closePath()
    ctx.stroke()

    state.lastFew.push(props.values)
    if (state.lastFew.length > 10) {
      state.lastFew = state.lastFew.slice(1)
    }
  }
}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}

interface POS {
  x: number
  y: number
}

const bottomGap = 48;

function drawPattern(ctx: CanvasRenderingContext2D, values: number[], depth: number, useRelative: boolean) {


  let minY = 0
  let maxY = 50
  minY = Math.min(...values);
  state.min = Math.round(minY * 100) / 100;
  maxY = Math.max(...values);
  state.max = Math.round(maxY * 100) / 100;

  if (useRelative) {
    minY = 0
    maxY = 10
  }

  if (props.fft == true) {

    state.top = values.map(v => v)
    state.top = state.top.sort((a, b) => b - a).map(v => values.indexOf(v))

    let masterPoll = 400 * 1000;
    let subSample = 32;
    let freq = masterPoll / subSample;
    let window = 1024;
    state.top = state.top.map(v => Math.round((v * freq / window) * 1) / 1).filter(v => v != 0)
  }


  ctx.lineWidth = 1
  ctx.strokeStyle = depth == -1 ? `rgba(255, 128, 0, 0.5)` : `rgba(0, 128, 255, 0)`;
  ctx.fillStyle = `rgba(255, 128, 10,${map_range(depth == -1 ? 20 : depth, 0, 20, 0, 0.5)})`;

  let w = ctx.canvas.width;
  let h = ctx.canvas.height;


  // ctx.moveTo(0, h / 1.25 - (values[0] - minY) / (maxY - minY) * (ctx.canvas.height) / 1.5)
  let lastX = 0;
  let lastY = h / 1.25 - (values[0] - minY) / (maxY - minY) * (ctx.canvas.height) / 1.5;
  let mass = w / (values.length)
  for (let i = 0; i < values.length; i++) {
    if (values[i] == maxY) {
      state.maxIndex = i;
    }
    let x = i * mass;
    let outline = 2
    let y = map_range(values[i], minY, maxY, 0, (ctx.canvas.height) / 1.5)
    // ctx.moveTo(lastX, lastY)
    ctx.fillRect(x - outline * 2, h / 1.25 - y, mass - outline * 2, h / 1.25 - y+10)
    // lastX = x;
    // lastY = y
  }


}

</script>

<template>
  <div>
    <h1></h1>
    <div class="canvas-group element">

      <div style="height: 2rem" class="d-flex gap-1 justify-content-between w-100">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ props.name }} <span
            class="text-muted">0-{{ props.values.length }}</span></div>
        <div class="d-flex gap-1 ">
          <div class="d-flex gap-2 tag label">
            <div>􀆇</div>
            <div>{{ state.max }}</div>
          </div>
          <div class="d-flex gap-2 tag label">
            <div>􀆈</div>
            <div>{{ state.min }}</div>
          </div>
          <div v-if="props.fft"></div>
          <div class="d-flex gap-2 tag tag label" v-if="props.fft" v-for="i in state.top.slice(0,3)">
            <div>Freq.</div>
            <div>{{ i }} Hz</div>
          </div>
        </div>
      </div>
      <div class="canvas-container">
        <canvas :id="`signal-${state.uuid}`" class="inner-canvas"></canvas>
      </div>
    </div>
  </div>
</template>

<style>
.bar {
  display: flex;
  justify-content: start;
  padding: 0.2rem;
  gap: 1rem;
}

.canvas-group {
  display: flex;
  flex-direction: column;
  justify-content: center;

  align-items: center;
  border-radius: 8px;
  background-color: hsla(214, 9%, 28%, 0.3);
  padding: 6px
}

.inner-canvas {
  width: 100%;
  height: 100%;

}

.canvas-container {
  display: flex;
  flex-direction: row;
  justify-content: center;
  width: 100%;
  height: 20rem;
  align-items: center;

}
</style>
