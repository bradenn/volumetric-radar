<script lang="ts" setup>

import {onMounted, onUnmounted, reactive} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
  name: string
  r: number[]
  t: number[]
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
  avgOffset: 2,
  uuid: uuidv4(),
})

onMounted(() => {
  configureCanvas()
  animate()
})

onUnmounted(() => {

})

function animate() {
  requestAnimationFrame(animate)
  draw();
}

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

  drawPattern(ctx, props.t, props.r, -1, false)
  ctx.beginPath()
  let mapAvg = new Map<number, number>()
  let depth = 0
  // for (const hKey in state.lastFew) {
  //   depth++
  //   // let arr = matchedFilter(state.lastFew[hKey], [0.25, 0.75, 0.25]);
  //   // drawPattern(ctx, arr, -2, false)
  //   for (let i = 4; i < arr.length; i++) {
  //     mapAvg.set(i, arr[i] + (mapAvg.get(i) || 0));
  //   }
  // }
  ctx.closePath()
  ctx.stroke()

  if (props.fft) {
    // let ou = [] as number[]
    // mapAvg.forEach(a => ou.push(a))
    // let m = Math.max(...ou)

    // let arr = matchedFilter(ou.slice(1), [0.25, 0.5, 0.25]);
    // ctx.beginPath()
    // drawPattern(ctx, arr.map(v => (v)), -1, false)
    // ctx.closePath()
    // ctx.stroke()

    // state.lastFew.push(props.values)
    // if (state.lastFew.length > 4) {
    //   state.lastFew = state.lastFew.slice(1)
    // }
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

function lerp(a: number, b: number, t: number): number {
  return (1 - t) * a + t * b;
}

function findLocalMaximas(arr: number[], threshold: number): number[] {
  const maximas: number[] = [];
  for (let i = 1; i < arr.length - 1; i++) {
    if (arr[i] > arr[i - 1] && arr[i] > arr[i + 1] && arr[i] > threshold) {
      maximas.push(i);
    }
  }
  return maximas;
}

function drawPattern(ctx: CanvasRenderingContext2D, t: number[], r: number[], depth: number, useRelative: boolean) {

  // values = matchedFilter(values, [0.25,0.5,0.75,0.5,0.25])
  let minY = 0
  let maxY = 50
  minY = Math.min(...r);
  state.min = Math.round(minY * 100) / 100;
  maxY = Math.max(...r);
  state.max = Math.round(maxY * 100) / 100;
  let begin = 0
  let slope = 0
  let ls = 0
  // let mxs = findLocalMaximas(values, 20)
  //
  //
  // values = values.slice(mxs[0])
  // if (useRelative) {
  //   minY = 0
  //   maxY = 10
  // }


  ctx.lineWidth = 1
  ctx.strokeStyle = 'rgba(255, 128, 0, 0.5)';

  let w = ctx.canvas.width;
  let h = ctx.canvas.height;

  let divisions = 128;
  let slice = (Math.PI * 2) / divisions
  ctx.beginPath()
  for (let i = 0; i < t.length; i++) {
    let x = Math.cos(t[i] / Math.PI) * map_range(r[i], minY, maxY, 0, w / 8)
    let y = Math.sin(t[i] / Math.PI) * map_range(r[i], minY, maxY, 0, w / 8)
    ctx.moveTo(w / 2, h / 2);
    ctx.lineTo(w / 2 + x, h / 2 + y);

  }
  ctx.closePath()
  ctx.stroke()
  // for (let i = 0; i < values.length; i++) {
  //
  //   // lastX = x;
  //   // lastY = y
  // }


}

</script>

<template>
  <div>
    <h1></h1>
    <div class="canvas-group element">

      <div class="d-flex gap-1 justify-content-between w-100" style="height: 2rem">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ props.name }} <span
            class="text-muted"></span></div>
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
          <div v-for="i in state.top.slice(0,3)" v-if="props.fft" class="d-flex gap-2 tag tag label">
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

<style scoped>
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
