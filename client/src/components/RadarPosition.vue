<script lang="ts" setup>

import {onMounted, onUnmounted, reactive} from "vue";
import * as THREE from "three";
import {BoxGeometry, RawShaderMaterial} from "three";
import {OrbitControls} from "three/examples/jsm/controls/OrbitControls";
import {UnrealBloomPass} from 'three/examples/jsm/postprocessing/UnrealBloomPass';
import {RenderPass} from "three/examples/jsm/postprocessing/RenderPass";
import {EffectComposer} from "three/examples/jsm/postprocessing/EffectComposer";
import {v4 as uuidv4} from "uuid";

interface RadarPosition {
    pan: number
    tilt: number
}

const props = defineProps<RadarPosition>()

const state = reactive({
    showFrustum: false,
    uuid: uuidv4(),
})

let renderer = {} as THREE.WebGLRenderer
let camera = {} as THREE.PerspectiveCamera
let composer = {} as EffectComposer
let scene = {} as THREE.Scene
let controls = {} as OrbitControls
let room = {} as THREE.Object3D
let frustum = {} as THREE.Object3D
let mesh = {} as THREE.Mesh<BoxGeometry, RawShaderMaterial>
let textMesh = {} as THREE.Mesh
let material = {} as THREE.RawShaderMaterial
let targets = new Map<string, THREE.Object3D>();


const params = {
    exposure: 0.4,
    bloomStrength: 0.5,
    bloomThreshold: 0.25,
    bloomRadius: 0.125
};

onMounted(() => {
    init()
})

onUnmounted(() => {
    scene.clear()
    renderer.dispose()
})

function toggleFrustum() {
    state.showFrustum = !state.showFrustum
    frustum.visible = state.showFrustum
}

function goToTop() {
    camera.position.set(0, 9, 0)
    controls.update()
}

function goToSide() {
    // controls.reset()
    // camera.
    camera.position.set(0, 5, 8)
    controls.update()
}

function init() {
    renderer = new THREE.WebGLRenderer({
        alpha: true,
        antialias: true
    });
    renderer.shadowMap.enabled = true;
    let element = document.getElementById(`beam-canvas-${state.uuid}`)
    if (!element) return


    renderer.setSize(element.clientWidth, element.clientHeight);
    renderer.setPixelRatio(window.devicePixelRatio);
    // renderer.localClippingEnabled = true;
    element.appendChild(renderer.domElement)
    let width = element.clientWidth
    let height = element.clientHeight
    renderer.autoClear = false;
    renderer.localClippingEnabled = true;
    camera = new THREE.PerspectiveCamera(50, width / height, 1, 100);
    controls = new OrbitControls(camera, renderer.domElement);
    scene = new THREE.Scene();
    setCamera(0, 5, 8)
    controls.enableDamping = true
    controls.dampingFactor = 0.1
    camera.setFocalLength(50)

    scene.background = new THREE.Color(0x191B1E);
    const renderScene = new RenderPass(scene, camera);
    const bloomPass = new UnrealBloomPass(new THREE.Vector2(window.innerWidth, window.innerHeight), 1.5, 0.4, 0.85);
    bloomPass.threshold = params.bloomThreshold;
    bloomPass.strength = params.bloomStrength;
    bloomPass.radius = params.bloomRadius;

    renderer.setClearColor(0x000000, 0);
    composer = new EffectComposer(renderer);
    composer.addPass(renderScene);
    composer.addPass(bloomPass);


    // const axesHelper = new THREE.AxesHelper(10);
    // axesHelper.setColors(new THREE.Color(255, 0, 0), new THREE.Color(0, 255, 0), new THREE.Color(0, 0, 255))
    // scene.add(axesHelper);
    controls.update()
    // controls.maxPolarAngle = (Math.PI / 180) * 95
    // controls.minPolarAngle = 0
    // controls.maxAzimuthAngle = (Math.PI / 180) * 90
    // controls.minAzimuthAngle = (Math.PI / 180) * -90
    // controls.enableZoom = false
    scene.add(new THREE.HemisphereLight(0xffffff, 0x000000, 1))
    const gridHelper = new THREE.GridHelper(10, 2)
    // scene.add(gridHelper)


    scene.rotateY((Math.PI / 180) * 90)


    goToTop();
    toggleFrustum()
    toggleFrustum()
    animate()
}

function setCamera(x: number, y: number, z: number) {
    camera.position.set(x, y, z);
    camera.lookAt(0, 1, 0);
}

function render() {

    // mesh.material.uniforms.cameraPos.value.copy( camera.position );
    if (textMesh.lookAt) {
        textMesh.lookAt(camera.position)
    }
    controls.update()
    // room.
    //
    // material.uniforms["time"].value+=0.025
    // mesh.material.uniforms.cameraPos.value.copy( camera.position );
    composer.render();
}

function animate() {
    requestAnimationFrame(animate);
    render()
}

</script>

<template>
    <div>
        <div :id="`beam-canvas-${state.uuid}`" class="inner-canvas">
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

.beam-buffer {

    width: 100%;
    background-color: hsla(214, 9%, 28%, 0.125);
    /*border: 1px solid red;*/

    border-radius: 3px !important;
    /*box-shadow: 0 0 4px 1px red;*/

}

.inner-canvas {

    margin: 6px;
    width: 12rem;
    height: 2.25rem;
    /*aspect-ratio: 1.5/1;!**!*/
    /*box-shadow: 0 0 4px 1px blue*/

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
