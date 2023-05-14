<script lang="ts" setup>


import {onMounted, onUnmounted, reactive, watchEffect} from "vue";
import {v4 as uuidv4} from "uuid";
import type {Metadata} from "@/types";

interface Datatype {
    name: string
    unit: Metadata
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
    draw()
    // animate()
})

onUnmounted(() => {
    state.canvas.remove()
})

// function animate() {
//   requestAnimationFrame(animate)
//   draw();
// }

watchEffect(() => {
    draw();
    return props.unit
})

function configureCanvas() {
    const _canvas = document.getElementById(`chirp-${state.uuid}`)
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

    let w = ctx.canvas.width;
    let h = ctx.canvas.height;


    let res = props.unit.chirp.resolution
    let duration = props.unit.chirp.duration
    let steps = props.unit.chirp.steps
    let padding = props.unit.chirp.padding

    ctx.clearRect(0, 0, w, h)
    ctx.strokeStyle = 'rgba(255,128,1,0.8)'
    ctx.beginPath()
    let lx = 0
    let ly = h

    let dx = w / steps

    for (let i = 0; i < steps; i++) {
        let x = map_range(i, 0, steps, 0, w)

        let y = map_range(i, 0, steps, h, 0)
        if (props.unit.chirp.padding > 0) {
            //     // dac->chirp.resolution / (double)(dac->chirp.steps)
            let resStep = Math.round(res / steps)
            let numChirps = Math.round(steps / padding)
            //
            //     let
            //     // (power % dac->chirp.padding) * (dac->stepResolution * (dac->chirp.steps/dac->chirp.padding))
            //
            let amp = (i % padding) * (resStep * numChirps)
            y = map_range(amp, 0, res, h, 0)
        }

        ctx.moveTo(x, y)
        ctx.lineTo(lx, ly)
        lx = x
        ly = y
    }

    ctx.closePath()
    ctx.stroke();


}


function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
    return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
}
</script>

<template>
    <div class="chirp-canvas element-fg">
        <canvas :id="`chirp-${state.uuid}`" class="chirp"></canvas>
    </div>
</template>

<style scoped>

.chirp {
    width: 100%;
    height: 100%;
}

.chirp-canvas {
    width: 12rem;
    height: 1.85rem;
}
</style>
