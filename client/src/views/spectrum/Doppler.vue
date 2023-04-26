<script setup lang="ts">


import {onMounted, onUnmounted, reactive} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
  name: string
  values0: number[]
  values1: number[]
}

let props = defineProps<Datatype>();
const cols = 400
const rows = props.values0.length
const maxBuff = rows * cols

const state = reactive({
  lastFew: [] as number[][],
  canvas: {} as HTMLCanvasElement,
  ctx: {} as CanvasRenderingContext2D,
  min: 0,
  max: 0,
  maxIndex: 0,
  buffers: [[], [], [], []] as number[][],
  top: [] as number[],
  high: [] as number[],
  low: [] as number[],
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

  // state.ctx.clearRect(0, 0,   state.ctx.canvas.width, state.ctx.canvas.height);

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

//
// watchEffect(() => {
//
//   state.buffers[0] = props.values0.slice(0, maxBuff)
//   // state.high.push(Math.max(...newBuf))
//   // state.low.push(Math.min(...newBuf))
//
//
//   return props.values0
// })


function draw() {
  let ctx = state.ctx;
  if (!ctx.canvas) return
  ctx.lineWidth = 2
  let w = ctx.canvas.width;
  let h = ctx.canvas.height;
  // ctx.clearRect(0, 0, w, h);

  // // drawLegend()


  drawPattern(ctx, props.values0, 'rgba(255,128,10,1)', h / 2 - h / 4)
}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}

let runningMin: number[] = []
let runningMax: number[] = []

let displacement = 0;
function drawPattern(ctx: CanvasRenderingContext2D, values: number[], color: string, yLevel: number) {

  let minY = 0
  let maxY = 30

  let adj = values

  minY = Math.min(...values);
  maxY = Math.max(...values);
  // runningMin.unshift(minY)
  // runningMax.unshift(maxY)
  // let runs = 10
  // minY = runningMin.slice(0, runs).reduce((a, b) => b + a) / runs
  // maxY = runningMax.slice(0, runs).reduce((a, b) => b + a) / runs


  ctx.lineWidth = 1
  ctx.strokeStyle = color;
  ctx.fillStyle = color;

  let w = ctx.canvas.width;
  let h = ctx.canvas.height;

  // ctx.beginPath()

  let dx = w / cols;
  let dy = h / rows;
  if (displacement <= 0) {
    ctx.save()
  }
  // let id = ctx.getImageData(0, 0, w, h);
  // let pixels = id.data;
  // state.buffers.unshift(props.values0)
  // if (state.buffers.length >= rows) {
  //   state.buffers = state.buffers.slice(0, rows)
  // }
  ctx.translate(dx, 0)
  displacement += dx;
  // console.log(displacement)
  if (displacement >= w) {
    ctx.restore()
    displacement = 0
  }
  //
  // for (let i = 0; i < rows; i++) {
  //   if (state.buffers.length <= i) break
  let rowSet = resampleData(props.values0, rows);
  for (let j = 0; j < rows; j++) {
    // let x = Math.floor(map_range(i, 0, state.buffers[j].length, 0, w))
    // let y = Math.floor(map_range(j, 0, state.buffers.length, 0, h))
    let e = map_range(rowSet[j], minY, maxY, 0, 1)
    // let r = 255 * e
    // let g = 255 - e * 255
    // let b = 255 * e
    // let off = (y * id.width + x) * 4;
    // pixels[off] = r;
    // pixels[off + 1] = g;
    // pixels[off + 2] = b;
    // pixels[off + 3] = 255;
    ctx.fillStyle = `hsl(${e * 360}, 50%, 50%)`;
    ctx.fillRect(0, j * dy, dx, dy)

  }

  // }

  // state.ctx.putImageData(id, 0, 0);

  // for (let i = 1; i < adj.length; i++) {
  //
  //   // ctx.moveTo(x, y + yLevel)
  //   // ctx.lineTo(lastX, lastY + yLevel)
  //   lastX = x;
  //   lastY = y;
  // }
  // ctx.stroke()
  // ctx.closePath()
}

</script>

<template>
  <div class="w-100">
    <div class="canvas-group element">

      <div style="height: 2rem" class="d-flex gap-1 justify-content-between w-100">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ props.name }} <span
            class="text-muted">0-{{ props.values0.length }}</span></div>
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
