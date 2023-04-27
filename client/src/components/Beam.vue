<script lang="ts" setup>

import {onMounted, onUnmounted, reactive, watchEffect} from "vue";
import * as THREE from "three";
import {BoxGeometry, Object3D, RawShaderMaterial, Vector2, Vector3} from "three";
import {OrbitControls} from "three/examples/jsm/controls/OrbitControls";
import {UnrealBloomPass} from 'three/examples/jsm/postprocessing/UnrealBloomPass';
import {RenderPass} from "three/examples/jsm/postprocessing/RenderPass";
import {EffectComposer} from "three/examples/jsm/postprocessing/EffectComposer";
import type {Zone} from "@/types";
import {v4 as uuidv4} from "uuid";
import {TextGeometry} from "three/examples/jsm/geometries/TextGeometry";
import {FontLoader} from "three/examples/jsm/loaders/FontLoader";

interface BeamProps {
    zone: Zone,
    data: number[],
    pitch: number,
    roll: number
}

const props = defineProps<BeamProps>()

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

watchEffect(() => {
    if (frustum.isObject3D) {
        // frustum.rotateX(props.roll * (Math.PI/180))
        frustum.rotation.set( (props.pitch-90) * (Math.PI/180), props.roll * (Math.PI/180),0)
        // frustum.rotateZ(props.roll)
    }
    return props.pitch
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


function drawFloorPlan(vertices: Vector2[]): THREE.Object3D {
    let points = vertices.map(v => new Vector3(v.x, v.y, 0));
    points.push(points[0])
    const geometrySpline = new THREE.BufferGeometry().setFromPoints(points);

    const line = new THREE.Line(geometrySpline, new THREE.LineBasicMaterial({
        color: 0x636366,
        // dashSize: 1 / 25,
        // gapSize: 1 / 25,
        opacity: 1,
        transparent: true
    }));
    line.computeLineDistances();

    return line
}

let clippingPlanes = []

function drawWalls(vertices: Vector2[], height: number): THREE.Object3D {
    let obj = new Object3D()

    for (let i = 0; i < vertices.length; i++) {
        let point = vertices[i];
        let buf = new THREE.BufferGeometry()
        buf.setFromPoints([new Vector3(point.x, point.y, 0), new Vector3(point.x, point.y, height)])

        const line = new THREE.Line(buf, new THREE.LineBasicMaterial({
            color: 0x636366,
            dashSize: 1 / 25,
            gapSize: 1 / 25,

        }));
        line.computeLineDistances();
        obj.add(line)
    }


    return obj
}

function drawFrustum(x: number, y: number, z: number, fx: number, fy: number): THREE.Object3D {
    let obj = new THREE.Object3D()

    let numPointsAzimuth = 12
    let numPointsElevation = 12
    let radius = 5.5
    let azimuthRotationOffset = (Math.PI / 180) * ((90 - fx) + 45)
    let elevationRotationOffset = (Math.PI / 180) * ((90 - fy / 2))
    let sliceAzimuth = ((Math.PI / 180) * fx) / numPointsAzimuth
    let sliceElevation = ((Math.PI / 180) * fy) / numPointsElevation

    let groundPlane = new THREE.Plane(new Vector3(0, 1, 0), 1 / 2)

    for (let j = 0; j < numPointsElevation; j++) {
        let azimuthPoints = []
        let topPoints = []
        azimuthPoints.push(new Vector3(0, 0, 0))
        for (let i = 0; i < numPointsAzimuth; i++) {
            let lx = Math.sin(sliceElevation * j + elevationRotationOffset) * Math.cos(sliceAzimuth * i + azimuthRotationOffset) * radius
            let ly = Math.sin(sliceElevation * j + elevationRotationOffset) * Math.sin(sliceAzimuth * i + azimuthRotationOffset) * radius
            let lz = Math.cos(sliceElevation * j + elevationRotationOffset) * radius
            azimuthPoints.push(new Vector3(lx, ly, lz))
            if (j == 0 || j == numPointsElevation - 1) {
                topPoints.push(new Vector3(lx, ly, lz))
                topPoints.push(new Vector3(0, 0, 0))
            }
        }
        azimuthPoints.push(new Vector3(0, 0, 0))

        let buf = new THREE.BufferGeometry()
        buf.setFromPoints(azimuthPoints)
        const line = new THREE.Line(buf, new THREE.LineDashedMaterial({
            color: 0x0A80FF,
            dashSize: 1 / 25,
            gapSize: 1 / 25,
            opacity: 0.5,
            transparent: true
        }));
        line.material.clippingPlanes = [groundPlane]
        line.computeLineDistances();
        obj.add(line)

        let buf2 = new THREE.BufferGeometry()
        buf2.setFromPoints(topPoints)
        const line2 = new THREE.Line(buf2, new THREE.LineDashedMaterial({
            color: 0x0A80FF,
            dashSize: 1 / 25,
            gapSize: 1 / 25,
            opacity: 0.5,
            transparent: true
        }));
        line2.computeLineDistances();
        line2.material.clippingPlanes = [groundPlane]
        obj.add(line2)
    }


    obj.translateX(x)
    obj.translateY(y)
    obj.translateZ(z)


    return obj
}

const particlesData = [];
let group;
// const vertexShader = /* glsl */`
// 				precision highp float;
// 		uniform mat4 modelViewMatrix;
// 		uniform mat4 projectionMatrix;
// 		uniform float time;
//
// 		attribute vec3 position;
// 		attribute vec2 uv;
// 		attribute vec3 translate;
//
// 		varying vec2 vUv;
// 		varying float vScale;
//
// 		void main() {
//
// 			vec4 mvPosition = modelViewMatrix * vec4( translate, 1.0 );
// 			vec3 trTime = vec3(translate.x + time,translate.y + time,translate.z - time);
// 			float scale = sin( trTime.z * 3.14159 * 0.25) * sin( -trTime.y * 3.14159 * 0.5);
// 			vScale = scale;
// 			scale = scale * 5.0 + 20.0;
// 			mvPosition.xyz += position * scale;
// 			vUv = uv;
// 			gl_Position = projectionMatrix * mvPosition;
//
// 		}`;
//
// const fragmentShader = /* glsl */`
// 					precision highp float;
//
// 		uniform sampler2D map;
//
// 		varying vec2 vUv;
// 		varying float vScale;
//
// 		// HSL to RGB Convertion helpers
// 		vec3 HUEtoRGB(float H){
// 			H = mod(H,1.0);
// 			float R = abs(H * 6.0 - 3.0) - 1.0;
// 			float G = 2.0 - abs(H * 6.0 - 2.0);
// 			float B = 2.0 - abs(H * 6.0 - 4.0);
// 			return clamp(vec3(R,G,B),0.0,1.0);
// 		}
//
// 		vec3 HSLtoRGB(vec3 HSL){
// 			vec3 RGB = HUEtoRGB(HSL.x);
// 			float C = (1.0 - abs(2.0 * HSL.z - 1.0)) * HSL.y;
// 			return (RGB - 0.5) * C + HSL.z;
// 		}
//
// 		void main() {
// 			vec4 diffuseColor = texture2D( map, vUv );
// 			gl_FragColor = vec4(vec3(0.5, 1.0, 0.04), diffuseColor.w );
//
// 			if ( diffuseColor.w < 0.5 ) discard;
// 		}
// 				`;
// function drawTargets() {
//   const circleGeometry = new THREE.CircleGeometry( 0.125/100, 6 );
//
//   let geometry = new THREE.InstancedBufferGeometry();
//   geometry.index = circleGeometry.index;
//   geometry.attributes = circleGeometry.attributes;
//
//   const particleCount = 100;
//
//   const translateArray = new Float32Array( particleCount * 3 );
//   let scale=1/2;
//   for ( let i = 0, i3 = 0, l = particleCount; i < l; i ++, i3 += 3 ) {
//
//     translateArray[ i3 + 0 ] = (Math.random() * 2 - 1)*scale;
//     translateArray[ i3 + 1 ] = (Math.random() * 2 - 1)*scale*2;
//     translateArray[ i3 + 2 ] = (Math.random() * 2 - 1)*scale;
//
//   }
//
//   geometry.setAttribute( 'translate', new THREE.InstancedBufferAttribute( translateArray, 3 ) );
//
//   material = new THREE.RawShaderMaterial( {
//     uniforms: {
//       'map': { value: new THREE.TextureLoader().load( '/img.png' ) },
//       'time': { value: 0.0 }
//     },
//     vertexShader: vertexShader,
//     fragmentShader: fragmentShader,
//     depthTest: true,
//     depthWrite: true
//   } );
//
//   mesh = new THREE.Mesh( geometry, material );
//   mesh.scale.set( scale,scale,scale );
//   scene.add( mesh );
//
// }

// function drawTargets() {
//   const size = 128;
//   const data = new Uint8Array( size * size * size );
//
//   let i = 0;
//   const scale = 0.05;
//   const perlin = new ImprovedNoise();
//   const vector = new THREE.Vector3();
//
//   for ( let z = 0; z < size; z ++ ) {
//
//     for ( let y = 0; y < size; y ++ ) {
//
//       for ( let x = 0; x < size; x ++ ) {
//
//         const d = 1.0 - vector.set( x, y, z ).subScalar( size / 2 ).divideScalar( size ).length();
//         data[ i ] = ( 128 + 128 * perlin.noise( x * scale / 1.5, y * scale, z * scale / 1.5 ) ) * d * d;
//         i ++;
//
//       }
//
//     }
//
//   }
//
//   const texture = new THREE.Data3DTexture( data, size, size, size );
//   texture.format = THREE.RedFormat;
//   texture.minFilter = THREE.LinearFilter;
//   texture.magFilter = THREE.LinearFilter;
//   texture.unpackAlignment = 1;
//   texture.needsUpdate = true;
//
//   // Material
//
//   const vertexShader = /* glsl */`
// 					in vec3 position;
// 					uniform mat4 modelMatrix;
// 					uniform mat4 modelViewMatrix;
// 					uniform mat4 projectionMatrix;
// 					uniform vec3 cameraPos;
// 					out vec3 vOrigin;
// 					out vec3 vDirection;
// 					void main() {
// 						vec4 mvPosition = modelViewMatrix * vec4( position, 1.0 );
// 						vOrigin = vec3( inverse( modelMatrix ) * vec4( cameraPos, 1.0 ) ).xyz;
// 						vDirection = position - vOrigin;
// 						gl_Position = projectionMatrix * mvPosition;
// 					}
// 				`;
//
//   const fragmentShader = /* glsl */`
// 					precision highp float;
// 					precision highp sampler3D;
// 					uniform mat4 modelViewMatrix;
// 					uniform mat4 projectionMatrix;
// 					in vec3 vOrigin;
// 					in vec3 vDirection;
// 					out vec4 color;
// 					uniform vec3 base;
// 					uniform sampler3D map;
// 					uniform float threshold;
// 					uniform float range;
// 					uniform float opacity;
// 					uniform float steps;
// 					uniform float frame;
// 					uint wang_hash(uint seed)
// 					{
// 							seed = (seed ^ 61u) ^ (seed >> 16u);
// 							seed *= 9u;
// 							seed = seed ^ (seed >> 4u);
// 							seed *= 0x27d4eb2du;
// 							seed = seed ^ (seed >> 15u);
// 							return seed;
// 					}
// 					float randomFloat(inout uint seed)
// 					{
// 							return float(wang_hash(seed)) / 4294967296.;
// 					}
// 					vec2 hitBox( vec3 orig, vec3 dir ) {
// 						const vec3 box_min = vec3( - 0.5 );
// 						const vec3 box_max = vec3( 0.5 );
// 						vec3 inv_dir = 1.0 / dir;
// 						vec3 tmin_tmp = ( box_min - orig ) * inv_dir;
// 						vec3 tmax_tmp = ( box_max - orig ) * inv_dir;
// 						vec3 tmin = min( tmin_tmp, tmax_tmp );
// 						vec3 tmax = max( tmin_tmp, tmax_tmp );
// 						float t0 = max( tmin.x, max( tmin.y, tmin.z ) );
// 						float t1 = min( tmax.x, min( tmax.y, tmax.z ) );
// 						return vec2( t0, t1 );
// 					}
// 					float sample1( vec3 p ) {
// 						return texture( map, p ).r;
// 					}
// 					float shading( vec3 coord ) {
// 						float step = 0.01;
// 						return sample1( coord + vec3( - step ) ) - sample1( coord + vec3( step ) );
// 					}
// 					void main(){
// 						vec3 rayDir = normalize( vDirection );
// 						vec2 bounds = hitBox( vOrigin, rayDir );
// 						if ( bounds.x > bounds.y ) discard;
// 						bounds.x = max( bounds.x, 0.0 );
// 						vec3 p = vOrigin + bounds.x * rayDir;
// 						vec3 inc = 1.0 / abs( rayDir );
// 						float delta = min( inc.x, min( inc.y, inc.z ) );
// 						delta /= steps;
// 						// Jitter
// 						// Nice little seed from
// 						// https://blog.demofox.org/2020/05/25/casual-shadertoy-path-tracing-1-basic-camera-diffuse-emissive/
// 						uint seed = uint( gl_FragCoord.x ) * uint( 1973 ) + uint( gl_FragCoord.y ) * uint( 9277 ) + uint( frame ) * uint( 26699 );
// 						vec3 size = vec3( textureSize( map, 0 ) );
// 						float randNum = randomFloat( seed ) * 2.0 - 1.0;
// 						p += rayDir * randNum * ( 1.0 / size );
// 						//
// 						vec4 ac = vec4( base, 0.0 );
// 						for ( float t = bounds.x; t < bounds.y; t += delta ) {
// 							float d = sample1( p + 0.5 );
// 							d = smoothstep( threshold - range, threshold + range, d ) * opacity;
// 							float col = shading( p + 0.5 ) * 3.0 + ( ( p.x + p.y ) * 0.25 ) + 0.2;
// 							ac.rgb += ( 1.0 - ac.a ) * d * col;
// 							ac.a += ( 1.0 - ac.a ) * d;
// 							if ( ac.a >= 0.95 ) break;
// 							p += rayDir * delta;
// 						}
// 						color = ac;
// 						if ( color.a == 0.0 ) discard;
// 					}
// 				`;
//
//   const geometry = new THREE.BoxGeometry( 1, 1, 1 );
//   const material = new THREE.RawShaderMaterial( {
//     glslVersion: THREE.GLSL3,
//     uniforms: {
//       base: { value: new THREE.Color( 0x798aa0 ) },
//       map: { value: texture },
//       cameraPos: { value: new THREE.Vector3() },
//       threshold: { value: 0.3 },
//       opacity: { value: 0.01 },
//       range: { value: 0.2 },
//       steps: { value: 200 },
//       frame: { value: 0 }
//     },
//     vertexShader,
//     fragmentShader,
//     side: THREE.BackSide,
//     transparent: true
//   } );
//
//   mesh = new THREE.Mesh( geometry, material );
//   scene.add( mesh );
//
//   //
//
//   const parameters = {
//     threshold: 0.1,
//     opacity: 0.25,
//     range: 0.2,
//     steps: 100
//   };
//
//   function update() {
//
//     material.uniforms.threshold.value = parameters.threshold;
//     material.uniforms.opacity.value = parameters.opacity;
//     material.uniforms.range.value = parameters.range;
//     material.uniforms.steps.value = parameters.steps;
//
//   }
// }

function drawRoom(vertices: Vector2[]): THREE.Object3D {
    let obj = new THREE.Object3D()
    obj.add(drawFloorPlan(vertices))
    let roof = drawFloorPlan(vertices)
    roof.translateZ(-1.6)
    obj.add(roof)
    obj.add(drawWalls(vertices, -1.6))

    let maxX = Math.max(...vertices.map(b => b.x))
    let minX = Math.min(...vertices.map(b => b.x))
    let maxY = Math.max(...vertices.map(b => b.y))
    let minY = Math.min(...vertices.map(b => b.y))

    let xc = -(maxX + minX) / 2
    let yc = -(maxY + minY) / 2

    let frx = 2.6364428386
    let fry = -2.3764444702117493
    let frz = -1.2

    frustum = drawFrustum(frx, fry, frz, 80, 34)
    obj.add(frustum)
    let pl = createPlaneFromPoints(vertices)
    obj.add(pl)

    // const pl =
    //
    //

    //
    //

    let clusterPoints: Vector3[] = []

    for (let i = 0; i < 10; i++) {
        clusterPoints.push(new Vector3(1 - Math.random() * 2, 1 - Math.random() * 2, 1 - Math.random() * 2))
    }

    let bxcp = new THREE.Box3()
    bxcp.setFromPoints(clusterPoints)


    let bg = new THREE.BufferGeometry()
    bg.setFromPoints(clusterPoints)

    // let pts = new THREE.Points(bg)
    // obj.add(pts)

    // let font = new Font();
    const loader = new FontLoader();
    loader.load('https://threejs.org/examples/fonts/helvetiker_regular.typeface.json', function (font) {
        let tg = new TextGeometry("Hello", {
            font: font,
            size: 0.5,
            height: 0.12,
            curveSegments: 12,
            bevelEnabled: false
        });
        // textMesh = new THREE.Mesh(tg, new THREE.MeshPhongMaterial())
        // tg.computeBoundingBox()
        // textMesh.translateX(-(tg.boundingBox?.max.x || 0)/2)
        // obj.add(textMesh)
    });


    let cyl = new THREE.CapsuleGeometry(0.25, 0.5, 2, 6)
    let cylMesh = new THREE.LineSegments(new THREE.WireframeGeometry(cyl))
    cylMesh.rotateX(Math.PI / 2)
    cylMesh.translateY(-1 / 2)
    cylMesh.translateX(-xc)
    cylMesh.translateZ(yc)
    // obj.add(cylMesh)


    obj.rotateX((Math.PI / 180) * 90)
    obj.translateX(xc)
    obj.translateY(yc)
    obj.translateZ(1.6 / 3)
    let ax = new THREE.AxesHelper();
    scene.add(ax)

    // let bx = new THREE.Box3()
    // bx.setFromPoints(vertices.map(v => new Vector3(xc+v.x, -1.2/2, yc+v.y)))
    // let bx3 = new THREE.Box3Helper(bxcp);
    // obj.add(bx3)

    // let cyl = new THREE.Cylindrical(0.5, 0, 3);


    return obj

}

function getShapeFromPoints(points: Vector2[]) {
    const shape = new THREE.Shape();
    shape.moveTo(points[0].x, points[0].y);
    for (let i = 1; i < points.length; i++) {
        shape.lineTo(points[i].x, points[i].y);
    }
    return shape;
}

function createPlaneFromPoints(points: Vector2[]) {
    const planeMaterial = new THREE.MeshBasicMaterial({
        opacity: 0.1,
        color: 0x535356,
        transparent: true
    })
    const shape = getShapeFromPoints(points);
    const geometry = new THREE.ExtrudeGeometry(shape, {
        depth: -0.025,

        bevelEnabled: false,
    });
    // geometry.rotateX((Math.PI/180)*(-90));
    const mesh = new THREE.Mesh(geometry, planeMaterial);

    return mesh;
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


    let bf: Vector2[] = []
    for (let i = 0; i < props.zone.corners.length; i++) {
        bf.push(new Vector2(props.zone.corners[i][0], props.zone.corners[i][1]))
    }
    let bfMax = Math.max(...props.zone.corners.map(c => c[0]))
    let bfMin = Math.min(...props.zone.corners.map(c => c[0]))
    room = drawRoom(bf)

    // drawTargets()
    scene.add(room)
    let scale = 3.5 / (bfMax + Math.abs(bfMin))
    scene.scale.set(scale, scale, scale)
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
        <div class="canvas-group element">
            <div class="d-flex gap-1 justify-content-between w-100" style="height: 1.6rem">
                <div class="d-flex gap-2 label d-flex flex-row align-items-center px-2 ">{{ props.zone.name }}</div>
                <div class="d-flex gap-1 ">
                    <div class="d-flex tag label" @click="toggleFrustum">
                        <div v-if="!state.showFrustum">Show Frustum</div>
                        <div v-else>Hide Frustum</div>
                    </div>
                    <div class="d-flex gap-2 tag label" @click="goToTop">
                        <div>Top</div>
                    </div>
                    <div class="d-flex gap-2 tag label" @click="goToSide">
                        <div>Side</div>
                    </div>

                </div>
            </div>

            <div class="beam-buffer">
                <div :id="`beam-canvas-${state.uuid}`" class="inner-canvas">
                </div>
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

.beam-buffer {

    width: 100%;
    background-color: hsla(214, 9%, 28%, 0.125);
    /*border: 1px solid red;*/

    border-radius: 3px !important;
    /*box-shadow: 0 0 4px 1px red;*/

}

.inner-canvas {

    margin: 6px;
    width: calc(100% - 6px * 2);
    aspect-ratio: 1.5/1;
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
