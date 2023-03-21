<script setup lang="ts">

import type {Unit} from "@/types";
import {formatFrequency} from "@/utils";
import OverWave from "@/views/spectrum/OverWave.vue";
import Tag from "@/components/Tag.vue";
import Header from "@/components/Header.vue";
import {reactive, watchEffect} from "vue";
import Divider from "@/components/Divider.vue";

interface UnitProps {
  name: string
  unit: Unit
}

const state = reactive({
  rate: 0.0,
  rateHistory: [] as number[]
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

</script>

<template>
  <Header :name="props.unit.metadata.name">
    <Tag :active="props.unit.metadata.connected?'label-success':'label-warning'" :value="props.unit.metadata.connected?'Connected':'Disconnected'"
         name="Unit"></Tag>

    <div class="d-flex gap-1 align-items-center">
      <Tag :value="`${formatFrequency(state.rate)}`" name="Rx Rate"></Tag>
      <Divider></Divider>
      <Tag :value="formatFrequency(props.unit.metadata.base)" name="Carrier"></Tag>
      <Tag :value="formatFrequency(props.unit.metadata.frequency/4)" name="Sample Rate"></Tag>
      <Tag :value="`${props.unit.metadata.samples}`" name="Samples"></Tag>
      <Divider></Divider>
      <Tag :value="`${props.unit.metadata.window}`" name="Frame Size"></Tag>
      <Divider></Divider>
      <Tag :value="`${props.unit.metadata.prf} µs`" name="PRF"></Tag>
      <Tag :value="`${props.unit.metadata.chirp} µs`" name="Pulse"></Tag>
    </div>
  </Header>

  <div v-for="channel in props.unit.channels" :key="props.unit.channels.indexOf(channel)"
       class="d-flex gap-2 flex-column pt-1">
    <div v-if="channel.spectrum" class="d-flex flex-column gap-2">
      <!--      -->
      <!--      -->
      <!--      -->
      <!--      -->
      <!--      3-->
      <!--      -->
      <!--      -->
      <!--      -->
      <!--      -->
      <!--      <Doppler :values0="channel.signalI" :values1="[]" name=""></Doppler>-->
      <!---->
      <!--      -->
      <!--     -->
      <!--      <FFT :spectrum="channel.spectrum" name="ss" :lut="channel.frequencies" :frequencies="channel.peaks"></FFT>-->
      <!--      -->
      <!--      -->
      <!--      -->
      <!--      -->
      <OverWave :name="`RX ${props.unit.channels.indexOf(channel)}`" :resample=false :samples="props.unit.metadata.window"
                :values0="channel.signalI" :values1="channel.signalQ"></OverWave>
      <!--      <FFT :spectrum="channel.spectrum" name="ss" :lut="channel.frequencies" :frequencies="channel.peaks" ></FFT>-->
      <!--      -->
      <!--      -->
      <!--      <Scanner :channel="channel.spectrum" name="dd" style="width: 100%"></Scanner>-->
      <!--      -->
      <!--     -->
      <!--      <FFT :spectrum="channel.spectrum" name="ss" :lut="channel.frequencies" :frequencies="channel.peaks"></FFT>-->
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
  <div v-if="props.unit.phase">
    <OverWave :fft="false" :resample="false" :samples="props.unit.phase.length" :values0="[]" :values1="props.unit.phase"
              name="Phase"></OverWave>
    <!--    <FFT :spectrum="props.unit.phase" name="ss" :lut="[]" :frequencies="[]"></FFT>-->
    <!--    -->
    <!--    <Polar style="width: 30rem" :pings="props.unit.phase" :t="props.unit.distance" :delta="80" :fft="false" name=""></Polar>-->
    <!--    -->
    <!--    -->
    <!--    -->
    <!--    -->
    <!--    -->
    <!--    -->
    <!--    -->
    <!--    -->
    <!--    -->
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
</template>

<style scoped>

</style>