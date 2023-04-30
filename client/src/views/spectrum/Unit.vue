<script setup lang="ts">

import type {Unit, Zone} from "@/types";
import {formatFrequency} from "@/utils";
import Tag from "@/components/Tag.vue";
import Header from "@/components/Header.vue";
import {onMounted, reactive, watchEffect} from "vue";
import Divider from "@/components/Divider.vue";
import StackWave from "@/views/spectrum/StackWave.vue";
import Beam from "@/components/Beam.vue";
import Selector from "@/components/Selector.vue";
import Polar from "@/components/Polar.vue";

interface UnitProps {
    name: string
    unit: Unit
}

const state = reactive({
    rate: 0.0,
    rateHistory: [] as number[],
    options: {
        prf: [] as number[],
        resolution: [] as number[],
        steps: [] as number[],
        duration: [] as number[],
    }
})

const bedroom = {
    name: "Bedroom",
    corners: [
        [0.00007071067811865477, 0.00007071067811865474],
        [2.0364675298172563, -2.036467529817257],
        [2.0364675298172563, -2.3764444702117493],
        [3.1364428386310497, -2.3764444702117498],
        [3.1364428386310506, 1.993616858877351],
        [0, 2.0199212311374914],
    ],
} as Zone

onMounted(() => {
    generateOptions()
})

watchEffect(() => {
    state.rateHistory.unshift(props.unit.rate)
    let sum = state.rateHistory.reduce((a, b) => b + a)
    state.rate = sum / state.rateHistory.length
    const rateLength = 100;
    if (state.rateHistory.length > rateLength) {
        state.rateHistory = state.rateHistory.slice(0, rateLength)
    }
    return props.unit.rate
})

const props = defineProps<UnitProps>()

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

    for (let i = 0; i < 24; i++) {
        let rs = Math.pow(2, i) / 2
        if (i <= 13 && rs >= 2) {
            state.options.resolution.push(rs);
            state.options.steps.push(rs);
        }
        let prf = i * 1250
        state.options.prf.push(prf)
        state.options.duration.push(prf)
    }
}

function changeWindow(value: any) {
    let val = value as number
    alert(value)
}

</script>

<template>
    <div class="d-flex flex-column gap-1">
        <Header :name="props.unit.metadata.name">
            <Tag :active="props.unit.metadata.connected?'label-success':'label-warning'"
                 :value="props.unit.metadata.connected?'Connected':'Disconnected'"
                 name="Unit"></Tag>

            <div class="d-flex gap-1 align-items-center">
                <Tag :value="`${formatFrequency(state.rate)}`" name="Rx Rate" style="width: 5rem"></Tag>
                <Divider></Divider>
                <Tag :value="formatFrequency(props.unit.metadata.base)" name="Carrier"></Tag>
                <Tag :value="formatFrequency(props.unit.metadata.frequency/4)" name="Sample Rate"></Tag>
                <Tag :value="`${props.unit.metadata.samples}`" name="Samples"></Tag>
                <Divider></Divider>
                <Tag :value="`${props.unit.metadata.window}`" name="Frame Size"></Tag>
                <Divider></Divider>
                <Tag :value="`${props.unit.metadata.prf} µs`" name="PRF"></Tag>
                <Tag :value="`${props.unit.metadata.chirp} µs`" name="Pulse"></Tag>
                <Divider></Divider>
                <Tag :value="`${(Math.round(props.unit.pan*100)/100).toFixed(2)}&deg;`" name="Pitch"></Tag>
                <Tag :value="`${(Math.round(props.unit.tilt*100)/100).toFixed(2)}&deg;`" name="Roll"></Tag>
                <Divider></Divider>
                <Tag :border="temperatureMap(props.unit.temperature)"
                     :value="`${(Math.round((props.unit.temperature * 9/5 + 32)*100)/100).toFixed(2)}&deg; F`" name="Temperature"></Tag>
                <Divider></Divider>
                <Tag :value="`${Math.round(props.unit.rssi)} dB`"
                     name="RSSI"></Tag>
            </div>

        </Header>
        <div class="element px-3">
            <div class="d-flex gap-1 align-items-center">
                <Selector :change="changeWindow" :options="[128,256,384,512,1024,2048]"
                          :value="`${props.unit.metadata.window}`" name="Frame Size"></Selector>
                <Divider></Divider>
                <Selector :change="changeWindow" :options="state.options.duration" :value="`${props.unit.metadata.chirp.duration}`"
                          name="Duration"
                          unit=" µs"></Selector>
                <Selector :change="changeWindow" :options="state.options.prf" :value="`${props.unit.metadata.chirp.prf}`"
                          name="PRF" unit=" µs"></Selector>
                <Selector :change="changeWindow" :options="state.options.steps" :value="`${props.unit.metadata.chirp.steps}`"
                          name="Chirp Steps" unit=" steps"></Selector>
                <Selector :change="changeWindow" :options="state.options.resolution" :value="`${props.unit.metadata.chirp.resolution}`"
                          name="Resolution"
                          unit=""></Selector>
                <Divider></Divider>
                <Selector :change="changeWindow" :options="[0,2.5,6,11]" :value="`${props.unit.metadata.sampling.attenuation}`"
                          name="Attenuation"
                          unit=" dB"></Selector>
                <Selector :change="changeWindow" :options="[4096,8192,16384,20*1024,20830]" :value="`${props.unit.metadata.sampling.frequency}`"
                          name="Sample Rate" unit=" Hz"></Selector>

            </div>
        </div>
        <Beam v-if=false :data="props.unit.phase" :pitch="props.unit.pan" :roll="props.unit.tilt"
              :zone="bedroom"></Beam>
        <div v-for="channel in props.unit.channels" v-if="true"
             :key="props.unit.channels.indexOf(channel)" class="d-flex gap-2 flex-column pt-1 " style="z-index: -1 !important;">
            <div v-if="channel.signalI" class="d-flex flex-column gap-2">
                <!--        -->
                <!--        <OverWave :name="`RX ${props.unit.channels.indexOf(channel)}`" :resample=false :samples="props.unit.metadata.window"-->
                <!--                              :values0="channel.signalI" :values1="channel.signalQ"></OverWave>-->

                <StackWave v-if=true :name="`RX ${props.unit.channels.indexOf(channel)}`" :resample=false
                           :samples="props.unit.metadata.window"
                           :values0="channel.signalI" :values1="channel.signalQ"></StackWave>
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
        <div v-if="true">
            <!--    <OverWave :fft="false" :resample="false" :samples="props.unit.phase.length" :values0="[]" :values1="props.unit.phase"-->
            <!--              name="Phase"></OverWave>-->
            <!--    -->
            <!--    <Polar style="width: 30rem" :pings="props.unit.phase" :t="props.unit.distance" :delta="80" :fft="false" name=""></Polar>-->
            <!--            <FFT :spectrum="props.unit.phase" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
            <Polar :delta="80" :fft="false" :pings="props.unit.phase" :t="props.unit.distance" name=""
                   style="width: 30rem"></Polar>
            <!--  <Doppler :values0="props.unit.phase.map(p => p).sort((a, b) => b-a)" :values1="[]" name=""></Doppler>-->
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

</style>