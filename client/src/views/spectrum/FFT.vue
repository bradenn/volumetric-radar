<script lang="ts" setup>


import {onMounted, onUnmounted, reactive} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
  name: string
  spectrum: number[]
  frequencies: number[]
  lut: number[]
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
  state.canvas.remove()
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
    // ctx.lineWidth = 2
    // ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    // drawLegend()
    let w = ctx.canvas.width;
    let h = ctx.canvas.height;
    // ctx.strokeStyle = "rgba(255,255,255,0.125)";
    // ctx.beginPath()
    // ctx.moveTo(0, h / 1.25)
    // ctx.lineTo(w, h / 1.25)
    // ctx.stroke()
    // ctx.closePath()
    if (!props.spectrum) return;

    // for (let i = 0; i < props.spectrum.length; i++) {
    //   ctx.beginPath()
    //   ctx.moveTo(i * scale, h / 1.25 - 2)
    //   ctx.lineTo(i * scale, h / 1.25 + 2)
    //   ctx.stroke()
    //   ctx.closePath()
    // }
    ctx.fillStyle = "rgba(0,0,0,0.3)"
    ctx.fillRect(0, 0, w, h)
    drawPattern(ctx, props.spectrum, -1, false)
}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}

let runningMin: Map<number, number[]> = new Map<number, number[]>()
let runningMax: Map<number, number[]> = new Map<number, number[]>()

function drawPattern(ctx: CanvasRenderingContext2D, values: number[], depth: number, useRelative: boolean) {

    let minY = 0
    let maxY = 50
    //
    // if (!runningMin.get(0)) {
    //   runningMin.set(0, [])
    // }
    //
    // if (!runningMax.get(0)) {
    //   runningMax.set(0, [])
    // }

    values = values
    minY = Math.min(...values);
    state.min = Math.round(minY * 100) / 100;
    maxY = Math.max(...values);
    // runningMin.get(0)?.unshift(minY)
    // runningMax.get(0)?.unshift(maxY)
    // let runs = 1
    // minY = (runningMin.get(0)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs
    // maxY = (runningMax.get(0)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs

    let w = ctx.canvas.width;
    let h = ctx.canvas.height;

    let lastX = 0;
    let mass = w / values.length
    ctx.lineWidth = mass

    let lastY = map_range(values[0], minY, maxY, h / 1.25, h / 1.25 - (ctx.canvas.height) / 1.5);
    ctx.beginPath()
    for (let i = 1; i < values.length; i++) {
        let x = i * mass;
        let y = map_range(values[i], minY, maxY, h / 1.25, h / 1.25 - (ctx.canvas.height) / 1.5)

        ctx.strokeStyle = `rgba(10, 128, 255, ${0.8})`;
        ctx.moveTo(x, h / 1.25)
        ctx.lineTo(x, y)
        lastX = x;
        lastY = y;
    }
    ctx.stroke()
    ctx.closePath()
    if (props.frequencies && props.lut) {
        state.top = props.frequencies.map(f => props.lut[f])
    }
    // ctx.putImageData(id, 0, 0);

    // ctx.fill()
    ctx.fillStyle = 'rgba(255,255,255, 0.75)';
    ctx.strokeStyle = 'rgba(255,255,255, 0.25)';
    ctx.font = "20px JetBrains Mono"
    for (let i = 0; i < props.frequencies.length; i++) {
        let ln = `${Math.round(props.lut[props.frequencies.indexOf(props.frequencies[i])] * 10) / 10}Hz`;
        let mt = ctx.measureText(ln);
        let x = props.frequencies[i] * mass
        ctx.beginPath()
        ctx.moveTo(x, h / 1.25 + 6)
        ctx.lineTo(x, h / 1.25 + mt.fontBoundingBoxAscent / 2)
        ctx.stroke()
        ctx.closePath()


        ctx.fillText(ln, x - mt.width / 2, h / 1.25 + mt.fontBoundingBoxAscent * 1.75)
    }

}

</script>

<template>
  <div class="w-100">

    <div class="canvas-group element">

      <div class="d-flex gap-1 justify-content-between w-100" style="height: 2rem">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ props.name }} <span
            class="text-muted">0-{{ props.spectrum?.length }}</span></div>
        <div class="d-flex gap-1 ">
          <div v-for="i in state.top.slice(0,4)" class="d-flex gap-2 tag tag label">
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
