<script setup lang="ts">

import type {Metadata, Unit, Zone} from "@/types";
import {formatFrequency} from "@/utils";
import Tag from "@/components/Tag.vue";
import Header from "@/components/Header.vue";
import {onBeforeMount, reactive, watch, watchEffect} from "vue";
import Divider from "@/components/Divider.vue";
import StackWave from "@/views/spectrum/StackWave.vue";
import Beam from "@/components/Beam.vue";
import Selector from "@/components/Selector.vue";
import Polar from "@/components/Polar.vue";
import axios from "axios";
import Chirp from "@/views/spectrum/Chirp.vue";

interface UnitProps {
    name: string
    unit: Unit
    mode: number
}

const state = reactive({
    rate: 0.0,
    rateHistory: [] as number[],
    options: {
        prf: [] as number[],
        resolution: [] as number[],
        steps: [] as number[],
        duration: [] as number[],
        padding: [] as number[],
        frequency: [] as number[],
    },
    mode: 0,
    config: {
        chirp: {},
        sampling: {},
    } as Metadata,
    preset: -1,
    live: {} as Metadata,
})

const bedroom = {
    name: "OCNL 120",
    corners: [{"x": 2, "y": 2}, {"x": 28, "y": 2}, {"x": 28, "y": 13}, {"x": 27, "y": 14}, {"x": 27, "y": 18}, {
        "x": 2,
        "y": 18
    }, {"x": 2, "y": 2}].map(d => [d.x / 3, d.y / 3]),
} as Zone
const props = defineProps<UnitProps>()

onBeforeMount(() => {
    state.live = props.unit.metadata
    state.config = props.unit.metadata
    generateOptions()
})

watchEffect(() => {
    if (state.preset == 0) {
        presetAngle()
    } else if (state.preset == 1) {
        presetMovement()
    }
    return state.preset
})

watchEffect(() => {
    state.config.duration = props.unit.metadata.duration
    state.config.rate = props.unit.metadata.rate
    return props.unit.metadata.duration
})

watch(props.unit.metadata, (metadata, old) => {
    if (metadata != old) {
        state.config = props.unit.metadata
    }
    // state.live = props.unit.metadata
})


function temperatureMap(temp: number): string {
    if (temp > 0 && temp <= 45) {
        return "success"
    } else if (temp > 45 && temp <= 70) {
        return "warning"
    } else if (temp > 70 && temp <= 100) {
        return "danger"
    } else if (temp > 100 && temp <= 150) {
        return "critical"
    }
    return ""
}

function generateOptions() {
    state.options.steps = []
    state.options.resolution = []
    state.options.prf = []
    state.options.duration = []
    state.options.frequency = []

    for (let i = 10; i <= 20; i++) {
        state.options.frequency.push(1024 * i)
    }

    for (let i = 1; i < 26; i++) {
        state.options.steps.push(25 * i);
    }
    for (let i = 0; i < 48; i++) {
        let rs = i * 5
        state.options.resolution.push(state.config.chirp.steps * i);


        state.options.padding.push(i * 5);
        let prf = i * 1250
        state.options.prf.push(prf)
        state.options.duration.push(prf)
    }
    state.options.resolution.push(...[2048, 4096]);
}

function changeWindow(value: any) {
    let val = value as number
    alert(value)
}

function presetAngle() {
    state.config.sampling = {
        frequency: 20480,
        samples: 1,
        attenuation: 3
    }

    state.config.chirp = {
        prf: 25000,
        duration: 25000,
        steps: 250,
        padding: 10,
        resolution: 250
    }
    sendUpdate()
}

function presetMovement() {
    state.config.sampling = {
        frequency: 20480,
        samples: 1,
        attenuation: 3
    }

    state.config.chirp = {
        prf: 25000,
        duration: 25000,
        steps: 250,
        padding: 10,
        resolution: 250
    }
    sendUpdate()
}

function sendUpdate() {
    axios.post("http://localhost:5043/update", state.config).then(res => console.log(res)).catch(res => console.log(res))
}

</script>

<template>
    <div class="d-flex flex-column gap-2">

        <Header :desc="props.unit.metadata.mac" :name="props.unit.metadata.name">
            <Tag :active="props.unit.metadata.connected?'label-success':'label-warning'"
                 :value="props.unit.metadata.connected?'Connected':'Disconnected'"
                 name="Unit"></Tag>


            <div class="d-flex gap-1 align-items-center">
                <Tag :value="`${formatFrequency(props.unit.metadata.rate)}`" name="Receive Rate"
                     style="width: 5rem"></Tag>
                <Divider></Divider>
                <Tag
                    :value="`${(Math.round(props.unit.metadata.duration)/1000 / (props.unit.metadata.chirp.padding > 0?props.unit.metadata.chirp.steps/props.unit.metadata.chirp.padding:1)).toFixed(3)} ms`"
                    name="Chirp Duration"
                    style="width: 5rem"></Tag>
                <Tag v-if="props.unit.metadata.chirp.padding > 0"
                     :value="`${props.unit.metadata.chirp.steps/props.unit.metadata.chirp.padding}`"
                     name="Interphase Chirps"></Tag>
                <Tag :value="`${(props.unit.metadata.chirp.prf/1000)/(1000/props.unit.metadata.sampling.frequency)}`"
                     name="Chirp Samples"></Tag>
                <Divider></Divider>
                <Tag :value="formatFrequency(props.unit.metadata.base)" name="Carrier"></Tag>
                <Divider></Divider>
                <Tag :value="`${(Math.round(props.unit.pan*100)/100).toFixed(2)}&deg;`" name="Pitch"></Tag>
                <Tag :value="`${(Math.round(props.unit.tilt*100)/100).toFixed(2)}&deg;`" name="Roll"></Tag>
                <Divider></Divider>
                <Tag name="Amplification" value="73 dB"></Tag>
                <Divider></Divider>
                <Tag :border="temperatureMap(props.unit.temperature)"
                     :value="`${(Math.round((props.unit.temperature * 9/5 + 32)*100)/100).toFixed(2)}&deg; F`"
                     name="Temperature"></Tag>
                <Divider></Divider>
                <Tag :value="`${Math.round(props.unit.rssi)} dB`"
                     name="Wi-Fi RSSI"></Tag>

            </div>

        </Header>

        <div class="element px-2 py-2" style="border-radius: 8px">
            <div class="d-flex gap-1 align-items-center">
                <Selector :change="(v) => {state.preset = v}" :labels="['Angle', 'Movement']"
                          :options="[0, 1]"
                          :value="`${state.preset}`"
                          name="Preset" unit=" µs">
                    <div v-if="state.preset == -1">Select</div>
                    <div v-else-if="state.preset == 0">Angle</div>
                    <div v-else-if="state.preset == 1">Movement</div>
                </Selector>
                <Divider></Divider>
                <Selector :change="(v) => {state.config.chirp.prf = v}" :options="state.options.prf"
                          :value="`${state.config.chirp.prf}`"
                          name="PRF" unit=" µs">
                    {{ state.config.chirp.prf }} µs
                </Selector>
                <Selector :change="(v) => {state.config.chirp.duration = v}" :options="state.options.duration"
                          :value="`${state.config.chirp.duration}`"
                          name="Duration"
                          unit=" µs">
                    {{ state.config.chirp.duration }} µs
                </Selector>
                <Divider></Divider>
                <Selector :change="(v) => {state.config.chirp.steps = v}" :options="state.options.steps"
                          :value="`${props.unit.metadata.chirp.steps}`"
                          name="Chirp Steps" unit=" steps">
                    {{ state.config.chirp.steps }}
                </Selector>
                <Selector :change="(v) => {state.config.chirp.resolution = v}" :options="state.options.resolution"
                          :value="`${props.unit.metadata.chirp.resolution}`"
                          name="Bandwidth"
                          unit="">
                    {{ formatFrequency((state.config.chirp.resolution / 4096) * 2.5 * 80e6) }}
                </Selector>
                <Selector :change="(v) => {state.config.chirp.padding = v}" :options="state.options.padding"
                          :value="`${state.config.chirp.padding}`"
                          name="Divisor" unit="">
                    {{ state.config.chirp.padding }}
                </Selector>
                <Divider></Divider>
                <Chirp :unit="props.unit.metadata" name="sd"></Chirp>
                <Divider></Divider>
                <Selector :change="(v) => {state.config.sampling.frequency = v}"
                          :options="state.options.frequency"
                          :value="`${state.config.sampling.frequency}`"
                          name="Frequency" unit="">
                    {{ formatFrequency(state.config.sampling.frequency) }}
                </Selector>
                <Selector :change="(v) => {state.config.sampling.samples = v}"
                          :options="[4096,8192,16384,20*1024,20830]"
                          :value="`${state.config.sampling.samples}`"
                          name="Samples" unit="">
                    {{ state.config.sampling.samples }}
                </Selector>
                <Selector :change="(v) => {state.config.sampling.attenuation = v}" :labels="['0 dB', '2.5 dB', '6 dB', '11 dB']"
                          :options="[0,1,2,3]"
                          :value="`${state.config.sampling.attenuation}`"
                          name="Attenuation"
                          unit=" dB">
                    <div v-if="state.config.sampling.attenuation == 0">0 dB</div>
                    <div v-else-if="state.config.sampling.attenuation == 1">2.5 dB</div>
                    <div v-else-if="state.config.sampling.attenuation == 2">6 dB</div>
                    <div v-else-if="state.config.sampling.attenuation == 3">11 dB</div>
                </Selector>
                <Divider></Divider>
                <Selector :change="(v) => {state.config.audible = v}"
                          :labels="['0', '10', '25', '50','75', '125', '250', '500', '1000']"
                          :options="[0, 10, 25, 50, 75, 125, 250, 500, 1000]"
                          :value="`${state.config.audible}`"
                          name="Audible"
                          unit="">
                    {{ state.config.audible }}
                </Selector>
                <div
                    v-if="JSON.stringify(props.unit.metadata).replace(' ', '') != JSON.stringify(state.config).replace(' ', '')"
                    class="d-flex align-items-center gap-1">
                    <Divider></Divider>
                    <div class="tag element-fg d-flex justify-content-center text-accent" style="height: 2.2rem"
                         @click="(e) => sendUpdate()">Apply
                    </div>
                </div>


            </div>
        </div>
        <!--        <pre v-text="props.unit.metadata"></pre>-->
        <!--        <pre v-text="state.config"></pre>-->

        <div v-if="props.mode === 1" class="">
            <Beam :data="props.unit.phase" :pitch="props.unit.pan-90" :roll="props.unit.tilt"
                  :x="2.75" :y="2" :zone="bedroom"></Beam>
        </div>
        <div v-if="props.mode === 0" class="channel-grid">
            <div v-for="channel in props.unit.channels" v-if="true"
                 :key="props.unit.channels.indexOf(channel)"
                 style="z-index: -1 !important;">
                <div v-if="channel.signalI" class="flex-grow-1">
                    <!--        -->
                    <!--        <OverWave :name="`RX ${props.unit.channels.indexOf(channel)}`" :resample=false :samples="props.unit.metadata.window"-->
                    <!--                              :values0="channel.signalI" :values1="channel.signalQ"></OverWave>-->
                    <!--                <FFT :spectrum="channel.spectrum" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                    <StackWave v-if=true :metadata="props.unit.metadata" :name="`Antenna ${props.unit.channels.indexOf(channel)+1}`"
                               :resample=false :samples="props.unit.samples"
                               :values0="channel.signalI" :values1="channel.signalQ"></StackWave>
                    <!--                <FFT :spectrum="channel.spectrum" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                    <!--                <Doppler :values0="channel.signalI" :values1="[]" name=""></Doppler>-->
                    <!--                -->
                    <!--                -->
                    <!--            <StackWave :name="`RX ${props.unit.channels.indexOf(channel)}`" :resample=false :samples="props.unit.metadata.window"-->
                    <!--                                                 :values0="channel.signalI" :values1="channel.signalQ"></StackWave>-->
                    <!--        <FFT :spectrum="channel.spectrum" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                    <!--        <Doppler :values0="channel.spectrum" :values1="[]" name=""></Doppler>-->
                    <!--        -->
                    <!--        <Doppler :values0="channel.spectrum" :values1="[]" name=""></Doppler>-->
                    <!--        <FFT :spectrum="channel.spectrum" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                    <!--        <MultiWave :channel="channel.signalI" name=""></MultiWave>-->
                    <!--        <FFT :spectrum="channel.signalQ" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                    <!--        <FFT :spectrum="channel.signalI" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                <!--        <FFT :spectrum="channel.signalQ" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
                <!--        -->
                <!--        -->
                <!--        <FFT :spectrum="channel.spectrum" name="ss" :lut="channel.frequencies" :frequencies="channel.peaks" ></FFT>-->
                <!--      <FFT :spectrum="channel.spectrum" name="ss" :lut="channel.frequencies" :frequencies="channel.peaks" ></FFT>-->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      <Scanner :channel="channel.spectrum" name="dd" style="width: 100%"></Scanner>-->
                <!--      -->
                <!--     -->
                <!--      -->
                <!--      <Doppler :values0="channel.signalI" :values1="[]" name=""></Doppler>-->

                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      >-->
                <!--     -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                <!--      -->
                    <!--      -->
                    <!--      -->
                    <!--      <Doppler :values0="channel.spectrum" :values1="[]" name=""></Doppler>-->
                    <!--      -->
                    <!--        -->
                    <!--    -->
                    <!--      <Doppler :values0="channel.spectrum" :values1="[]" name=""></Doppler>-->
                    <!--    <Doppler :values0="channel.signalQ" :values1="[]" name=""></Doppler>-->
                </div>

            </div>
        </div>
        <div v-if="props.mode == 2">
            <!--    -->
            <!--    -->
            <StackWave v-if=false :metadata="props.unit.metadata" :name="`Antenna `"
                       :resample=false :samples="props.unit.samples"
                       :values0="props.unit.distance" :values1="props.unit.phase"></StackWave>
            <!--            <OverWave :fft="false" :resample="false" :samples="props.unit.distance.length" :values0="[]" :values1="props.unit.distance" name="Phase"></OverWave>-->
            <!--           -->
            <!--    <Polar style="width: 30rem" :pings="props.unit.phase" :t="props.unit.distance" :delta="80" :fft="false" name=""></Polar>-->
            <!--            <FFT :spectrum="props.unit.phase" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
            <Polar :delta="80" :fft="false" :pings="props.unit.phase" :t="[]" name=""
                   style="width:20rem"></Polar>
            <!--            -->
            <!--  -->
            <!--  -->
            <!--    -->
        </div>

        <!--  -->
        <!--  -->
        <!--  >-->
        <!--  <Doppler :values0="props.unit.phase.map(p => p).sort((a, b) => b-a)" :values1="[]" name=""></Doppler>-->
        <!--  -->
        <!--  -->
    </div>
</template>

<style scoped>
.channel-grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(10rem, 1fr));
    grid-template-rows: repeat(1, minmax(10rem, 1fr));
    grid-gap: 0.33rem;
}
</style>