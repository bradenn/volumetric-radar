<script lang="ts" setup>

import {onMounted, onUnmounted, reactive, watchEffect} from "vue";
import {v4 as uuidv4} from "uuid";

interface Datatype {
    name: string
    pings?: number[]
    t?: number[]
    delta: number,
    landscape?: boolean
}

let props = defineProps<Datatype>();

const state = reactive({
    lastFew: [] as number[][],
    canvas: {} as HTMLCanvasElement,
    ctx: {} as CanvasRenderingContext2D,
    bins: Array(80),
    resetTime: 0,
    min: 0,
    max: 0,
    maxIndex: 0,
    top: [] as number[],
    avgOffset: 2,
    uuid: uuidv4(),
})

onMounted(() => {
    configureCanvas()
    for (let i = 0; i < 80; i++) {
        state.bins[i] = 0
    }
    animate()
})

onUnmounted(() => {

})

watchEffect(() => {
    if (!props.pings) return
    for (let i = 0; i < props.pings.length; i++) {
        let deg = props.pings[i]
        // let dist = map_range(props.t[i], minT, maxT, ((r - 1) * slice) / 8, (r - 1) * slice)
        let rnd = Math.round(deg)
        if (rnd >= -40 && rnd <= 40) {
            state.bins[rnd + 40]++
        }
        // let labelX = Math.cos(degToRad(angleCenter + angle)) * dist
        // let labelY = Math.sin(degToRad(angleCenter + angle)) * dist
        //
        // // ctx.fillText(dist, offsetX + labelX - me.width / 2, offsetY + labelY);
        // ctx.fillRect(offsetX + labelX, offsetY + labelY, 10, 10)
    }
    return props.pings
})

function animate() {
    requestAnimationFrame(animate)
    draw();
}

let runningMin: Map<number, number[]> = new Map<number, number[]>()
let runningMax: Map<number, number[]> = new Map<number, number[]>()

function drawRad(ctx: CanvasRenderingContext2D, w: number, h: number, r: number, ang: number, rot?: number) {
    ctx.strokeStyle = "rgb(99,99,102)";
    ctx.fillStyle = "rgb(10,132,255,0.8)";
    let angleCenter = -90 + (!rot ? 0 : rot)
    let angleStart = angleCenter - ang / 2
    let angleStop = angleCenter + ang / 2
    let degToRad = (deg: number): number => deg * ((Math.PI) / 180)
    let slice = ((h) / r - 2)
    let offsetX = w / 2;
    let offsetY = h - (h - slice * r) / 2;
    ctx.font = "normal 22px JetBrains Mono"
    for (let i = 1; i < r; i++) {

        ctx.beginPath()
        ctx.arc(offsetX, offsetY, i * slice, degToRad(angleStart), degToRad(angleStop), false)
        if ((i) % 2 == 0 && i != 0) {
            let dist = `${i}`
            let me = ctx.measureText(dist)
            let edgeLeftX = Math.cos(degToRad(angleStop)) * slice * i
            let edgeLeftY = Math.sin(degToRad(angleStop)) * slice * i
            ctx.fillText(dist, offsetX + edgeLeftX + me.actualBoundingBoxAscent, offsetY + edgeLeftY + me.actualBoundingBoxAscent);
        }
        ctx.stroke()
        ctx.closePath()
    }

    ctx.fillStyle = "rgb(255,128,10,0.6)";
    let numLabels = Math.ceil(ang / 10);
    let degPerLabel = (ang / 2) / (numLabels / 2)
    for (let i = 0; i <= numLabels; i++) {
        let labelX = Math.cos(degToRad(angleStart + (ang / numLabels) * i)) * (r - 1 / 1.5) * slice
        let labelY = Math.sin(degToRad(angleStart + (ang / numLabels) * i)) * (r - 1 / 1.5) * slice
        let dist = `${degPerLabel * (i - numLabels / 2)}`
        let me = ctx.measureText(dist)
        let edgeLeftX = Math.cos(degToRad(angleStart + (ang / numLabels) * i)) * (r - 1) * slice
        let edgeLeftY = Math.sin(degToRad(angleStart + (ang / numLabels) * i)) * (r - 1) * slice
        ctx.fillText(dist, offsetX + labelX - me.width / 2, offsetY + labelY);
        ctx.beginPath()
        ctx.moveTo(offsetX, offsetY)
        ctx.lineTo(offsetX + edgeLeftX, offsetY + edgeLeftY)
        ctx.stroke()
        ctx.closePath()
    }

    let maxT = -1
    let minT = 1
    let values;
    // if (props.pings) {
    //     values = props.t
    //     // let runs = 20
    //     maxT = Math.max(...props.pings)
    //     state.max = maxT
    //     minT = Math.min(...props.pings)
    //     state.min = minT
    //     // if (!runningMin.get(0)) {
    //     //   runningMin.set(0, [])
    //     // }
    //     //
    //     // if (!runningMax.get(0)) {
    //     //   runningMax.set(0, [])
    //     // }
    //     // runningMin.get(0)?.unshift(minT)
    //     // runningMax.get(0)?.unshift(maxT)
    //     // minT = (runningMin.get(0)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs
    //     // maxT = (runningMax.get(0)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs
    // }


    if (props.pings && props.t) {
        let pings = props.pings
        let avg = 0


        for (let j = 0; j < 80; j++) {
            let angle = map_range(j, 0, 80, -40, 40)
            let labelX = Math.cos(degToRad(angleCenter + angle)) * ((r - 1) * slice)
            let labelY = Math.sin(degToRad(angleCenter + angle)) * ((r - 1) * slice);

            let dist = map_range(state.bins[j], 0, 100, (r - 1) * slice, ((r - 1) * slice) / 2)
            let px = Math.cos(degToRad(angleCenter + angle)) * dist
            let py = Math.sin(degToRad(angleCenter + angle)) * dist
            ctx.beginPath()
            ctx.moveTo(offsetX + labelX, offsetY + labelY)
            ctx.lineTo(offsetX + px, offsetY + py)
            ctx.closePath()
            ctx.stroke()
            state.bins[j] = 0
        }

        // avg /= props.t.length
        // let dist = map_range(avg, minT, maxT, ((r - 1) * slice) / 8, (r - 1) * slice)
        // let labelX = Math.cos(degToRad(angleCenter)) * dist
        // let labelY = Math.sin(degToRad(angleCenter)) * dist
        // // ctx.fillText(dist, offsetX + labelX - me.width / 2, offsetY + labelY);
        // ctx.fillRect(offsetX + labelX, offsetY + labelY, 20, 20)

    }
    //
    //
    // ctx.lineWidth = 2;
    // ctx.beginPath()
    // ctx.moveTo(offsetX, offsetY)
    //
    //
    // // ctx.lineTo(offsetX + edgeLeftX, offsetY - edgeLeftY)
    // ctx.stroke()
    // ctx.closePath()
    // ctx.beginPath()
    // ctx.moveTo(offsetX, offsetY)
    //
    // let edgeRightX = Math.cos(degToRad(angleStop + 180)) * (r - 1) * slice
    // let edgeRightY = Math.sin(degToRad(angleStop + 180)) * (r - 1) * slice
    //
    // ctx.lineTo(offsetX + edgeRightX, offsetY - edgeRightY)
    // ctx.stroke()
    // ctx.closePath()

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

    drawRad(ctx, w, h, 16, props.delta, 0);


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

            <div class="d-flex gap-1 justify-content-between w-100" style="height: 1.6rem">
                <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2 ">{{ props.name }} <span
                    class="text-muted"></span></div>
                <div class="d-flex gap-1 ">
                    <div class="d-flex gap-2 tag label">
                        <div>&Delta;f</div>
                        <div>{{ props.delta }}&deg;</div>
                    </div>
                    <div class="d-flex gap-2 tag label">
                        <div>min</div>
                        <div>{{ state.min }}&deg;</div>
                    </div>
                    <div class="d-flex gap-2 tag label">
                        <div>max</div>
                        <div>{{ state.max }}&deg;</div>
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
