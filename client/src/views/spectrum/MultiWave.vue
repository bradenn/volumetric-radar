<script setup lang="ts">


import * as THREE from "three";
import {EffectComposer} from "three/examples/jsm/postprocessing/EffectComposer";
import {OrbitControls} from "three/examples/jsm/controls/OrbitControls";
import {RenderPass} from "three/examples/jsm/postprocessing/RenderPass";
import {UnrealBloomPass} from "three/examples/jsm/postprocessing/UnrealBloomPass";
import {Line2} from 'three/addons/lines/Line2.js';
import {LineMaterial} from 'three/addons/lines/LineMaterial.js';
import {LineGeometry} from 'three/addons/lines/LineGeometry.js';

import {onMounted, onUnmounted, reactive, watchEffect} from "vue";
import {v4 as uuidv4} from "uuid";

interface ScannerProps {
  channel: number[]
  name: string
}

const state = reactive({
  uuid: uuidv4(),
  settings: {
    scaleX: 1,
    scaleY: 1,
    window: 1
  },
  running: false,
})


const props = defineProps<ScannerProps>()

let renderer = {} as THREE.WebGLRenderer
let camera = {} as THREE.OrthographicCamera
let composer = {} as EffectComposer
let scene = {} as THREE.Scene
let controls = {} as OrbitControls
let core = {} as THREE.Object3D
let objects = {} as THREE.Object3D[];
let lineGeometry = {} as LineGeometry

onMounted(() => {
  init()
  updateLine()
  state.running = true
})

watchEffect(() => {
  if (state.running) {
    updateLine()
  }
  return props.channel
})

onUnmounted(() => {
  composer.dispose()
  lineGeometry.dispose()
  renderer.dispose()
})

const bufferSize = 4096 * 3;
let width = 0, height = 0

function map_range(value: number, low1: number, high1: number, low2: number, high2: number) {
  return low2 + (high2 - low2) * (value - low1) / (high1 - low1);
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

function updateLine() {

  const points = [] as number[]
  const colors = [] as number[]

  // lineGeometry.setPositions(points)
  // lineGeometry.setColors(colors

  let max = Math.max(Math.max(...props.channel), 1)
  let min = Math.min(...props.channel)
  let int = resampleData(props.channel, width)

  for (let i = 0; i < int.length; i++) {
    // points.push(i, (props.channel[i])*10, 0);
    let h = map_range(int[i], min, max, 0, 1)
    // let h = Math.sin(dx * i)
    points.push(i, -h * height / 4, 0);
    let r = map_range(Math.abs(h), 0, 1, 1, 0.8)
    let g = map_range(Math.abs(h), 0, 1, 0.5, 0.5)
    let b = map_range(Math.abs(h), 0, 1, 12 / 255, 12 / 255)
    colors.push(r, g, b);

  }
  lineGeometry.dispose()
  lineGeometry.setPositions(points)
  lineGeometry.setColors(colors)
  // lineGeometry.setAttribute('position', new THREE.Float32BufferAttribute(points, 3));
  // lineGeometry.setAttribute('color', new THREE.Float32BufferAttribute([1,0,1], 3));
}

const timeline: THREE.Object3D = {} as THREE.Object3D

function drawTimeline() {


  timeline.add()
}

function drawToolbar() {

}

function drawScale() {

}

function drawFrame() {

}


function init() {
  renderer = new THREE.WebGLRenderer({
    alpha: true,
    antialias: false
  });
  // renderer.shadowMap.enabled = true;
  let element = document.getElementById(`scanner-canvas-${state.uuid}`)
  if (!element) return


  renderer.setSize(element.clientWidth, element.clientHeight);
  renderer.setPixelRatio(window.devicePixelRatio * 2);
  // renderer.localClippingEnabled = true;
  element.appendChild(renderer.domElement)
  width = element.clientWidth
  height = element.clientHeight
  renderer.autoClear = false;
  // renderer.localClippingEnabled = true;
  camera = new THREE.OrthographicCamera(-width / 2, width / 2, height / 2, -height / 2, -1000, 1000);
  camera.lookAt(new THREE.Vector3(0, 0, 1))
  // camera.position.set(width/2,height/2,0)
  camera.up.set(0, 1, 0)
  scene = new THREE.Scene();
  const params = {
    exposure: 2,
    bloomStrength: 1,
    bloomThreshold: 0.2,
    bloomRadius: 0.25
  };

  scene.background = new THREE.Color(0x191B1E);
  const renderScene = new RenderPass(scene, camera);
  const bloomPass = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 1, 0.4, 0.85);
  bloomPass.threshold = params.bloomThreshold;
  bloomPass.strength = params.bloomStrength;
  bloomPass.radius = params.bloomRadius;

  renderer.setClearColor(0x000000, 0);
  composer = new EffectComposer(renderer);
  composer.addPass(renderScene);
  composer.addPass(bloomPass);


  lineGeometry = new LineGeometry();
  updateLine()


  let lineMaterial = new LineMaterial({

    color: 0xffffff,
    linewidth: 1, // in world units with size attenuation, pixels otherwise
    vertexColors: true,


    // resolution: 3,  // to be set by renderer, eventually
    dashed: false,
    alphaToCoverage: false,

  });
  lineMaterial.resolution.set(width * 2, height * 2);

  let line = new Line2(lineGeometry, lineMaterial);
  line.computeLineDistances();
  line.scale.set(1, 1, 1);
  // const lineMaterial = new THREE.LineBasicMaterial({color: 0Xff800a});
  updateLine()
  objects[0] = line
  objects[0].position.x = 0
  objects[0].position.y = 0
  objects[0].position.z = 0;
  objects[0].scale.set(1, 1, 1)

  // camera.lookAt(objects[0].position)

  scene.add(objects[0])

  let gridGeometry = new THREE.BufferGeometry();
  const gridMaterial = new THREE.LineBasicMaterial({color: 0xaaaaaa, opacity: 0.2, transparent: true});
  gridGeometry.setFromPoints([new THREE.Vector3(-width / 2, 0, 0), new THREE.Vector3(width / 2, 0, 0)])
  objects[1] = new THREE.Line(gridGeometry, gridMaterial);
  objects[1].renderOrder = 0
  scene.add(objects[1])

  let ah = new THREE.AxesHelper(width / 6)
  ah.position.x = 0
  ah.position.y = 0
  // scene.rotateX(Math.PI/2)
  // scene.add(ah)


  updateLine()
  animate()
}


function scroll(e: WheelEvent) {
  e.preventDefault()
  if (Math.abs(e.deltaY) < Math.abs(e.deltaX)) {
    state.settings.scaleX -= e.deltaX <= 0 ? -0.05 : 0.05
    state.settings.window = state.settings.scaleX
    state.settings.scaleX = Math.max(state.settings.scaleX, 0.1)
  } else {
    state.settings.scaleY -= e.deltaY <= 0 ? -0.05 : 0.05
    state.settings.scaleY = Math.max(state.settings.scaleY, 0.1)
  }

}

function setCamera(x: number, y: number, z: number) {
  camera.position.set(x, y, z);
  camera.lookAt(1, 0, 1);
}

function render() {
  // updateLine()
  lineGeometry.computeBoundingBox()
  if (lineGeometry.boundingBox) {
    objects[0].scale.set(width / lineGeometry.boundingBox.max.x, -state.settings.scaleY, 0)
    lineGeometry.computeBoundingBox()
    objects[0].position.x = -width / 2 - state.settings.window * state.settings.window * 10
    objects[0].position.y = 0
  }
  composer.render();
}

function animate() {
  requestAnimationFrame(animate);
  render()
}


</script>


<template>
  <div>
    <div class=" element d-flex flex-column p-2 gap-1">

      <div style="height: 2rem" class="d-flex gap-1 justify-content-between w-100">
        <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2">{{ name }} <span
            class="text-muted">Lines</span></div>
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
      <div class="scanner" @wheel="scroll">
        <div :id="`scanner-canvas-${state.uuid}`" class="h-100">
        </div>
      </div>
    </div>
  </div>


</template>


<style scoped lang="scss">

$toolbar-height: 3rem;

.scanner-timeline-shuttle {
  border-radius: 4px;
  width: 3rem;
  height: calc($toolbar-height - 8px);
  background-color: hsla(214, 9%, 28%, 0.2);
  border: 1px solid hsla(214, 9%, 28%, 0.3);
  box-shadow: inset 0px 0px 6px 1px hsla(214, 9%, 28%, 0.25);
  margin-top: 4px;
  //marg
}

.scanner-timeline {
  display: flex;
  justify-content: start;
  align-items: center;
  border-radius: 6px;
  width: 100%;
  height: $toolbar-height;
  background-color: hsla(214, 9%, 20%, 0.3);
  border: 1px solid hsla(214, 9%, 28%, 0.5);
  box-shadow: inset 0px 0px 6px 1px hsla(214, 9%, 28%, 0.25);
}

.scanner {
  border-radius: 6px;
  width: 100%;
  height: 20rem;
}
</style>