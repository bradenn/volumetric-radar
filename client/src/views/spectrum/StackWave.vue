<script lang="ts" setup>


import {onMounted, onUnmounted, reactive} from "vue";
import {v4 as uuidv4} from "uuid";
import Tag from "@/components/Tag.vue";
import Divider from "@/components/Divider.vue";
import type {Metadata} from "@/types";

interface Datatype {
    name: string
    values0: number[]
    values1: number[]
    samples: number
    metadata: Metadata
    resample: boolean
}

let props = defineProps<Datatype>();

let maxBuff = props.samples

interface Range {
    min: string,
    max: string,
    rms: string,
    key: number
}

const state = reactive({
    ranges: [{min: '0', max: '0', rms: '0', key: 0}, {min: '0', max: '0', rms: '0', key: 1}] as Range[],
    lastFew: [] as number[][],
    canvas: {} as HTMLCanvasElement,
    ctx: {} as CanvasRenderingContext2D,
    min: new Map<number, number>(),
    max: new Map<number, number>(),
    maxIndex: 0,
    buffers: [[], [], [], []] as number[][],
    top: [] as number[],
    fade: [] as number[],
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
    ctx.fillStyle = "rgba(127,127,127,0.1)"
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height)
    ctx.lineWidth = 0.5
    ctx.strokeStyle = `rgba(255, 255, 255, 0.3)`;
    ctx.beginPath()
    ctx.moveTo(0, h / 2)
    ctx.lineTo(w, h / 2)
    ctx.closePath()
    ctx.stroke()
    // ctx.beginPath()
    // ctx.moveTo(0, h - 4)
    // ctx.lineTo(w, h - 4)
    // ctx.closePath()
    // ctx.stroke()

    ctx.font = "normal 18px JetBrains Mono"
    ctx.fillStyle = "rgba(255,255,255,0.5)"
    ctx.fillText("In-Phase", 10, 30)
    ctx.fillText("Quadrature", 10, h / 2 + 30)
    let maxI1 = `Min ${state.ranges[0].min} mV`
    let maxQ1 = `Max ${state.ranges[0].max} mV`
    let maxI2 = `Min ${state.ranges[1].min} mV`
    let maxQ2 = `Max ${state.ranges[1].max} mV`
    let mxI1 = ctx.measureText(maxI1)
    let mxQ1 = ctx.measureText(maxQ1)
    let mxI2 = ctx.measureText(maxI2)
    let mxQ2 = ctx.measureText(maxQ2)
    ctx.fillText(maxI1, w - mxI1.actualBoundingBoxRight - mxQ1.actualBoundingBoxRight - 40, h / 2 + 30)
    ctx.fillText(maxQ1, w - mxQ1.actualBoundingBoxRight - 20, h / 2 + 30)


    ctx.fillText(maxI2, w - mxI2.actualBoundingBoxRight - mxQ2.actualBoundingBoxRight - 40, 30)
    ctx.fillText(maxQ2, w - mxQ2.actualBoundingBoxRight - 20, 30)
    state.buffers[0] = props.values0
    state.buffers[1] = props.values1


    drawPattern(ctx, 0, 'rgba(255,128,10,1)', h / 2)
    drawPattern(ctx, 1, 'rgba(20,140,255,1)', h)


}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
    return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}


let runningMin: Map<number, number[]> = new Map<number, number[]>()
let runningMax: Map<number, number[]> = new Map<number, number[]>()

function summarizeSignal(signal: number[], n: number): number[] {
    const chunkSize = Math.ceil(signal.length / n);
    const result: number[] = [];

    for (let i = 0; i < signal.length; i += chunkSize) {
        const chunk = signal.slice(i, i + chunkSize);
        const sum = chunk.reduce((a, b) => a + b, 0);
        result.push(sum / chunkSize);
    }

    return result;
}

function drawPattern(ctx: CanvasRenderingContext2D, values: number, color: string, yLevel: number) {

    let minY = 0
    let maxY = 1270

    if (!runningMin.get(values)) {
        runningMin.set(values, [])
    }

    if (!runningMax.get(values)) {
        runningMax.set(values, [])
    }

    let adj = props.resample ? summarizeSignal(state.buffers[values], ctx.canvas.width) : state.buffers[values]

    minY = Math.min(...adj)
    maxY = Math.max(...adj)
    runningMin.get(values)?.unshift(minY)
    runningMax.get(values)?.unshift(maxY)

    let runs = 10
    minY = (runningMin.get(values)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs
    maxY = (runningMax.get(values)?.slice(0, runs).reduce((a, b) => b + a) || 0) / runs

    state.ranges = state.ranges.map(r => r.key === values ? {
        min: (Math.round(minY * 100) / 100).toFixed(2),
        max: (Math.round(maxY * 100) / 100).toFixed(2),
        rms: (Math.round((maxY - minY) * 100) / 100).toFixed(2),
        key: values
    } : r)
    // maxY = 5
    // minY = 1

    ctx.lineWidth = 1
    ctx.strokeStyle = color;
    ctx.fillStyle = color;

    let w = ctx.canvas.width;
    let h = ctx.canvas.height;

    let lastX = 0;
    let mass = w / (adj.length)

    let lastY = Math.max(map_range(adj[0], minY, maxY, 20, h / 2 - 60), 2)
    ctx.beginPath()
    for (let i = 1; i < adj.length; i++) {
        let x = i * mass;
        let y = Math.max(map_range(adj[i], minY, maxY, 20, h / 2 - 60), 2)
        ctx.moveTo(lastX, (yLevel) - lastY)
        ctx.lineTo(x, (yLevel) - y)
        lastX = x;
        lastY = y;
    }
    ctx.closePath()
    ctx.stroke()
}

</script>

<template>
    <div class="w-100">
        <div class="canvas-group element" style="z-index: 1 !important;">

            <div class="d-flex gap-1 justify-content-between w-100" style="">
                <div class="d-flex align-items-start flex-column justify-content-center">
                    <div class="d-flex gap-2 label-c1 label d-flex flex-row align-items-center px-2">{{ props.name }}
                    </div>
                    <div class="d-flex gap-2 label-c5 label-o3 px-2 font-monospace">Time Domain
                    </div>

                </div>

                <div class="d-flex gap-1 align-items-center">
                    <Tag :name="`Duration`" :value="`${(props.samples/props.metadata.sampling.frequency) * 1000.0} ms`"
                         style="width: 4rem"></Tag>
                    <Tag :name="`Samples`" :value="`${props.samples}`"
                         style="width: 4rem"></Tag>
                    <Divider></Divider>
                    <div v-for="r in state.ranges" :key="state.ranges.indexOf(r)">
                        <div class="d-flex gap-1 align-items-center">
                            <!--                            <Tag :value="`${r.min} mV`" name="vMin" style="width: 6rem"></Tag>-->
                            <!--                            <Tag :value="`${r.max} mV`" name="vMax" style="width: 6rem"></Tag>-->
                            <Tag :name="`${state.ranges.indexOf(r)==0?'In-Phase':'Quadrature'} Δv`"
                                 :value="`${r.rms} mV`"
                                 style="width: 6rem"></Tag>

                            <Divider v-if="state.ranges.indexOf(r) === 0"></Divider>
                        </div>
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
//background-color: hsla(214, 9%, 28%, 0.3);
    padding: 6px
}

.inner-canvas {
    width: 100%;
    height: 100%;
    border-radius: 4px;
}

.canvas-container {
    display: flex;
    flex-direction: row;
    justify-content: center;
    width: 100%;
    height: 28rem;
    align-items: center;
    background-color: transparent;
    margin-top: 6px;
    border-radius: 4px;

}
</style>
