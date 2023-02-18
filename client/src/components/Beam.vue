<script lang="ts" setup>

import {onMounted, onUnmounted, reactive} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
  name: string
  r?: number[]
  t?: number[]
  d?: number[]
  deltaX: number,
  deltaY: number,
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

function projectPoint(point: [number, number, number]): [number, number] {
  const [x, y, z] = point;
  const projectedX = x / (1 + z);
  const projectedY = y / (1 + z);
  return [projectedX, projectedY];
}

function drawRad(ctx: CanvasRenderingContext2D, w: number, h: number, r: number, ang: number, rot?: number) {
  ctx.strokeStyle = "rgb(63,63,63)";
  ctx.fillStyle = "rgb(10,128,255,0.4)";
  let angleCenter = -90 + (!rot ? 0 : rot)
  let angleStart = angleCenter - ang / 2
  let angleStop = angleCenter + ang / 2
  let degToRad = (deg: number): number => deg * ((Math.PI) / 180)
  let slice = ((h) / r - 2)
  let offsetX = w / 2;
  let offsetY = h - (h - slice * r) / 2;
  ctx.font = "normal 22px JetBrains Mono"
  for (let i = 0; i < r; i++) {

    ctx.beginPath()
    ctx.arc(offsetX, offsetY, i * slice, degToRad(angleStart), degToRad(angleStop), false)
    if ((i) % 2 == 0 && i != 0) {
      let dist = `${i}`
      let me = ctx.measureText(dist)
      // let edgeLeftX = Math.cos(degToRad(angleStart + 180)) * slice*i
      // let edgeLeftY = Math.sin(degToRad(angleStart + 180)) * slice*i
      ctx.fillText(dist, offsetX - me.width / 2, offsetY - i * slice - me.actualBoundingBoxAscent / 2);
    }
    ctx.stroke()
    ctx.closePath()
  }


  ctx.lineWidth = 2;
  ctx.beginPath()
  ctx.moveTo(offsetX, offsetY)

  let edgeLeftX = Math.cos(degToRad(angleStart + 180)) * (r - 1) * slice
  let edgeLeftY = Math.sin(degToRad(angleStart + 180)) * (r - 1) * slice

  ctx.lineTo(offsetX + edgeLeftX, offsetY - edgeLeftY)
  ctx.stroke()
  ctx.closePath()
  ctx.beginPath()
  ctx.moveTo(offsetX, offsetY)

  let edgeRightX = Math.cos(degToRad(angleStop + 180)) * (r - 1) * slice
  let edgeRightY = Math.sin(degToRad(angleStop + 180)) * (r - 1) * slice

  ctx.lineTo(offsetX + edgeRightX, offsetY - edgeRightY)
  ctx.stroke()
  ctx.closePath()

  let scale = 25;

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

function draw() {
  let ctx = state.ctx;
  if (!ctx.canvas) return
  ctx.lineWidth = 2
  ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);

  let w = ctx.canvas.width;
  let h = ctx.canvas.height;

  drawRad(ctx, w, h, 16, 12, 0);


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
  //
  // drawPattern(ctx, props.t, props.r, -1, false)
  // ctx.beginPath()
  // let mapAvg = new Map<number, number>()
  // let depth = 0
  // // for (const hKey in state.lastFew) {
  // //   depth++
  // //   // let arr = matchedFilter(state.lastFew[hKey], [0.25, 0.75, 0.25]);
  // //   // drawPattern(ctx, arr, -2, false)
  // //   for (let i = 4; i < arr.length; i++) {
  // //     mapAvg.set(i, arr[i] + (mapAvg.get(i) || 0));
  // //   }
  // // }
  // ctx.closePath()
  // ctx.stroke()

  // if (props.fft) {
  //   // let ou = [] as number[]
  //   // mapAvg.forEach(a => ou.push(a))
  //   // let m = Math.max(...ou)
  //
  //   // let arr = matchedFilter(ou.slice(1), [0.25, 0.5, 0.25]);
  //   // ctx.beginPath()
  //   // drawPattern(ctx, arr.map(v => (v)), -1, false)
  //   // ctx.closePath()
  //   // ctx.stroke()
  //
  //   // state.lastFew.push(props.values)
  //   // if (state.lastFew.length > 4) {
  //   //   state.lastFew = state.lastFew.slice(1)
  //   // }
  // }
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
    <div class="canvas-group element">

      <div class="d-flex gap-1 justify-content-between w-100" style="height: 2rem">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ props.name }} <span
            class="text-muted"></span></div>
        <div class="d-flex gap-1 ">
          <div class="d-flex gap-2 tag label">
            <div>&Delta;x</div>
            <div>{{ props.deltaX }}&deg;</div>
          </div>
          <div class="d-flex gap-2 tag label">
            <div>&Delta;y</div>
            <div>{{ props.deltaY }}&deg;</div>
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
  gap: 6px;
  align-items: center;
  border-radius: 8px;
  background-color: hsla(214, 9%, 28%, 0.3);
  padding: 6px;


}

.inner-canvas {
  width: 100%;
  aspect-ratio: 1.25/1;

  background-color: hsla(214, 9%, 28%, 0.125);
  border-radius: 3px;
}

.canvas-container {
  display: flex;
  flex-direction: row;
  justify-content: center;

  width: 100%;
  height: 100%;

  align-items: center;


}
</style>
