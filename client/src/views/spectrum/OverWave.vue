<script setup lang="ts">


import {onMounted, onUnmounted, reactive, watchEffect} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
  name: string
  values0: number[]
  values1: number[]
  samples: number
  resample: boolean
}

let props = defineProps<Datatype>();

let maxBuff = props.samples

const state = reactive({
  lastFew: [] as number[][],
  canvas: {} as HTMLCanvasElement,
  ctx: {} as CanvasRenderingContext2D,
  min: 0,
  max: 0,
  maxIndex: 0,
  buffers: [[], [], [], []] as number[][],
  top: [] as number[],
  avgOffset: 2,
  uuid: uuidv4(),
  currentTime: 0,
  chirpPosition: 0,
})

onMounted(() => {
  configureCanvas()
  animate()
})

let anim = 0

onUnmounted(() => {
  cancelAnimationFrame(anim)
})

function animate() {
  anim = requestAnimationFrame(animate)
  draw();
}

function configureCanvas() {
  const _canvas = document.getElementById(`signal-${state.uuid}`)
  state.canvas = _canvas as HTMLCanvasElement
  state.ctx = state.canvas.getContext("2d", {}) as CanvasRenderingContext2D

  // state.ctx.filter = 'brightness(1000%)';
  let scale = 2
  state.ctx.scale(scale, scale)

  state.ctx.canvas.width = state.canvas.clientWidth * scale
  state.canvas.width = state.canvas.clientWidth * scale
  state.ctx.canvas.height = state.canvas.clientHeight * scale
  state.canvas.height = state.canvas.clientHeight * scale

  draw()
}

function cubicSplineInterpolation(points: number[][], resolution: number): number[][] {
  const n = points.length;
  const x = points.map(p => p[0]);
  const y = points.map(p => p[1]);
  const h = [];

  for (let i = 0; i < n - 1; i++) {
    h.push(x[i + 1] - x[i]);
  }

  const alpha = [];
  for (let i = 1; i < n - 1; i++) {
    alpha.push((3 / h[i]) * (y[i + 1] - y[i]) - (3 / h[i - 1]) * (y[i] - y[i - 1]));
  }

  const l = [1];
  const mu = [0];
  const z = [0];
  for (let i = 1; i < n - 1; i++) {
    l.push(2 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1]);
    mu.push(h[i] / l[i]);
    z.push((alpha[i - 1] - h[i - 1] * z[i - 1]) / l[i]);
  }

  l.push(1);
  z.push(0);

  const b = [];
  const c = [0];
  const d = [];
  for (let i = n - 2; i >= 0; i--) {
    c.unshift(z[i] - mu[i] * c[0]);
    b.unshift((y[i + 1] - y[i]) / h[i] - h[i] * (c[0] + 2 * c[1]) / 3);
    d.unshift((c[0] - c[1]) / (3 * h[i]));
  }

  const result = [];
  for (let i = 0; i < n - 1; i++) {
    const step = h[i] / resolution;
    for (let j = 0; j < resolution; j++) {
      const t = j * step;
      const x0 = x[i] + t;
      const y0 = y[i] + b[i] * t + c[i] * t ** 2 + d[i] * t ** 3;
      result.push([x0, y0]);
    }
  }

  result.push([x[n - 1], y[n - 1]]);
  return result;
}

function resampleData(data: number[], targetWidth: number): number[] {
  const currentWidth = data.length;

  // If the target width is smaller than the current width, downsample the data
  if (targetWidth < currentWidth) {
    const factor = currentWidth / targetWidth;
    const downsampled = [];

    for (let i = 0; i < targetWidth; i++) {
      const start = Math.floor(i * factor);
      const end = Math.floor((i + 1) * factor);
      const sum = data.slice(start, end).reduce((acc, d) => acc + d, 0);
      downsampled.push(sum / (end - start));
    }

    return downsampled;
  }

  // If the target width is larger than the current width, interpolate the data
  if (targetWidth > currentWidth) {
    const points = data.map((d, i) => [i, d]);
    const interpolated = cubicSplineInterpolation(points, targetWidth / currentWidth);
    return interpolated.map(p => p[1]);
  }

  // If the target width is the same as the current width, return the original data
  return data;
}

watchEffect(() => {
  let vs = props.values0.map(a => a)
  vs.push(...state.buffers[0])
  state.buffers[0] = vs.slice(0, maxBuff)

  return props.values0
})

watchEffect(() => {
  let vs = props.values1.map(a => a) as number[]
  state.buffers[1].unshift(...vs)

  state.buffers[1] = state.buffers[1].slice(0, maxBuff)

  return props.values1
})

function draw() {
  let ctx = state.ctx;
  if (!ctx.canvas) return
  ctx.lineWidth = 2

  ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
  // drawLegend()
  let w = ctx.canvas.width;
  let h = ctx.canvas.height;
  // ctx.strokeStyle = "rgba(255,255,255,0.125)";
  // ctx.beginPath()
  // ctx.moveTo(0, h / 1.25)
  // ctx.lineTo(w, h / 1.25)
  // ctx.stroke()
  // ctx.closePath()
  //
  // let scale = 25;
  //
  // for (let i = 0; i < w / scale; i++) {
  //   ctx.beginPath()
  //   ctx.moveTo(i * scale, h / 1.25 - 2)
  //   ctx.lineTo(i * scale, h / 1.25 + 2)
  //   ctx.stroke()
  //   ctx.closePath()
  // }


  drawPattern(ctx, 0, 'rgba(255,128,10,1)', h / 2 - h / 4)
  drawPattern(ctx, 1, 'rgba(255,128,10,1)', h / 2 + h / 4)
}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}


let runningMin: Map<number, number[]> = new Map<number, number[]>()
let runningMax: Map<number, number[]> = new Map<number, number[]>()

function drawPattern(ctx: CanvasRenderingContext2D, values: number, color: string, yLevel: number) {

  let minY = 0
  let maxY = 1270

  if (!runningMin.get(values)) {
    runningMin.set(values, [])
  }

  if (!runningMax.get(values)) {
    runningMax.set(values, [])
  }

  let adj = props.resample?resampleData(state.buffers[values], ctx.canvas.width):state.buffers[values]
  minY = Math.min(...adj);
  state.min = Math.round(minY * 100) / 100;
  maxY = Math.max(...adj) / 2;
  runningMin.get(values)?.unshift(minY)
  runningMax.get(values)?.unshift(maxY)
  let runs = 100
  minY = (runningMin.get(values)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs
  maxY = (runningMax.get(values)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs


  state.max = Math.round(maxY * 100) / 100;

  maxY = Math.max(maxY, 1)

  ctx.lineWidth = 1
  ctx.strokeStyle = color;
  ctx.fillStyle = color;

  let w = ctx.canvas.width;
  let h = ctx.canvas.height;

  let lastX = 0;
  let mass = w / (adj.length)

  let lastY = Math.max(map_range(adj[0], minY, maxY, 0, h / 8), 2)
  ctx.beginPath()
  for (let i = 1; i < adj.length; i++) {
    let x = i * mass;
    let y = Math.max(map_range(adj[i], minY, maxY, 0, h / 8), 2)
    ctx.moveTo(x, -y + yLevel)
    ctx.lineTo(lastX, -lastY + yLevel)
    lastX = x;
    lastY = y;
  }
  ctx.closePath()
  ctx.stroke()
}

</script>

<template>
  <div class="w-100">
    <div class="canvas-group element">

      <div style="height: 2rem" class="d-flex gap-1 justify-content-between w-100">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ props.name }} <span
            class="text-muted">I: {{ state.buffers[0].length }} Q: {{ state.buffers[1].length }}</span></div>
        <div class="d-flex gap-1 ">
          <div class="d-flex gap-2 tag label">
            <div>􀆇</div>
            <div>{{ state.max }} mV</div>
          </div>
          <div class="d-flex gap-2 tag label">
            <div>􀆈</div>
            <div>{{ state.min }} mV</div>
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
  height: 22rem;
  align-items: center;
  background-color: black;
  margin-top: 6px;
  border-radius: 4px;

}
</style>
