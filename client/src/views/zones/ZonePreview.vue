<script setup lang="ts">
import {onMounted, onUnmounted, reactive} from "vue";
import {v4 as uuidv4} from "uuid";
import {Canvas, Point, Space} from "@/graphicsUtils";


const state = reactive({
  lastFew: [] as number[][],
  canvas: {} as HTMLCanvasElement,
  ctx: {} as CanvasRenderingContext2D,
  min: 0,
  max: 0,
  maxIndex: 0,
  data: [] as Point[],
  top: [] as number[],
  cvs: new Canvas("zone-preview", render),
  avgOffset: 2,
  posX: 0,
  space: {} as Space,
  posY: 0.,
  uuid: uuidv4(),
})


onMounted(() => {
  state.cvs.init()

  document.addEventListener("keydown", keyDown)

  animate()
  // configureCanvas()
  // animate()
})
onUnmounted(() => {
  state.cvs.destroy()
})

function animate() {
  requestAnimationFrame(animate)
  if (state.cvs) {
    state.cvs.render()
  }

}

let space = new Space(0, 0, 14)

function reset() {
  return space.clear();
}

function render(ctx: CanvasRenderingContext2D): void {

  // ctx.fillRect(10, 10, 100, 100)'
  ctx.save()
  ctx.translate(state.posX, state.posY)
  // state.cvs.compass(ctx, 100)
  ctx.restore()
  ctx.save()
  ctx.translate(ctx.canvas.width - 120, 120)
  state.cvs.compass(ctx, 100)
  ctx.restore()

  ctx.save()

  state.cvs.origin(ctx)
  space.draw(ctx)
  state.data= []
  state.data = space.verticies
  ctx.restore()
  if (space.width == 0) {
    space = new Space(ctx.canvas.width, ctx.canvas.height, 26)
  }

}

function positionClick(e: MouseEvent) {
  state.posX = e.offsetX * 2
  state.posY = e.offsetY * 2
  space.toggleVertex(state.posX, state.posY)

}

function positionUnclick(e: MouseEvent) {
  e.preventDefault()
  e.stopPropagation()
  state.posX = e.offsetX * 2
  state.posY = e.offsetY * 2
  space.toggleVertexOff(state.posX, state.posY)
}

function zoom(e: WheelEvent) {
  e.preventDefault()

  space.size += e.deltaY > 0 ? -0.125 : 0.125
    console.log(e)
}

function positionTrack(e: MouseEvent) {

  state.posX = e.offsetX * 2
  state.posY = e.offsetY * 2
  space.moveCursor(state.posX, state.posY)
}

function keyDown(e: KeyboardEvent) {
  switch (e.key) {
    case "Escape":
      space.setEditMode(false)
      break
    case "e":
      space.setEditMode(!space.editMode)

  }
  if (e.key === "Escape") {
  }
}


function configureCanvas() {
  const _canvas = document.getElementById(`zone-preview-${state.uuid}`)
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
  // drawLegend()
  let w = ctx.canvas.width;
  let h = ctx.canvas.height;
  ctx.strokeStyle = "rgba(255,255,255,0.125)";


}

</script>

<template>
    <div class="element d-flex flex-column gap-2 px-2 py-2 mt-2" style="height: 75vh">
        <div class="d-flex w-100 justify-content-between">
            <input class="text-box" placeholder="Untitled Zone" type="text"/>
            <div class="d-flex gap-1">
                <div class="surface button"><i class="fa-solid fa-floppy-disk"></i></div>
                <div class="surface button" @click="reset"><i class="fa-solid fa-trash-can"></i></div>
            </div>
        </div>
        <canvas :id="state.cvs.Id" class="inner-canvas w-100 h-100 surface" style="" tabindex=0 @contextmenu="positionUnclick"
                @mousedown="positionClick" @mousemove="positionTrack"
                @wheel="zoom"></canvas>
    </div>
    {{state.data}}
</template>

<style scoped>

.button {
  border-radius: 6px;
  width: 3rem;
  height: 2rem;
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 0.8rem;
  color: rgba(100,100,92,0.8);
}

.button:hover {
  border-radius: 6px;
  width: 3rem;
  height: 2rem;
  display: flex;
  justify-content: center;
  align-items: center;
  font-size: 0.8rem;
  color: rgba(255,128,12,0.6);
  border: 1px solid rgba(255,128,12,0.2);
  background-color: rgba(255,128,12,0.2) !important;
}

.surface {
  background-color: hsla(214, 9%, 28%, 0.2) !important;
  border-radius: 6px;
  border: 1px solid transparent;
}

.surface:focus {
  border: 1px solid hsla(214, 9%, 28%, 0.6);
}

.surface:focus-visible {
  border: 1px solid hsla(214, 9%, 28%, 0.6);
  outline: none;
}

.element:not(.text-box) {
  border-radius: 14px;
  padding: 8px 8px !important;
}
.text-box {
  border-radius: 6px;
  background-color: hsla(214, 9%, 28%, 0.24);
  font-family: "JetBrains Mono",serif;
  font-size: 0.9rem;
  color: rgba(255,255,255,0.8) !important;
  padding: 2px 8px;
  border: 1px solid transparent;
}
.text-box:focus-visible{
  border-radius: 6px;
  background-color: hsla(214, 9%, 28%, 0.3);
  border: none;
  font-family: "JetBrains Mono",serif;
  font-size: 0.9rem;
  color: rgba(255,255,255,0.8) !important;
  padding: 2px 8px;
  border: 1px solid hsla(214, 9%, 28%, 0.6);
}

.text-box:active, .text-box:hover, .text-box:focus-visible {
  /*box-shadow: none!important;*/
  outline: none;
  /*border-bottom: 3px dashed rgba(255,128,0, 0.8);*/
}
</style>