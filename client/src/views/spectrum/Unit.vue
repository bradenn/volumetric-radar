<script setup lang="ts">

import type {Unit} from "@/types";
import {formatFrequency} from "@/utils";
import Doppler from "@/views/spectrum/Doppler.vue";
import OverWave from "@/views/spectrum/OverWave.vue";
import Polar from "@/components/Polar.vue";
import FFT from "@/views/spectrum/FFT.vue";

interface UnitProps {
  name: string
  unit: Unit
}

const props = defineProps<UnitProps>()

</script>

<template>
  <div class="element p-2 py-2">
    <div class="d-flex gap-1 align-items-center justify-content-between w-100">
      <div class="label-w500 labe-c3 lh-1 px-2">{{ props.name }}</div>
      <div class="px-2"></div>
      <div class="d-flex gap-1 align-items-center">
        <div class="d-flex gap-2 tag label">
          <div class="text-center">Carrier {{ formatFrequency(props.unit.metadata.base) }}</div>
        </div>
        <div class="mx-2" style="width: 1px; height: 1.25rem; background-color: rgba(255,255,255,0.2);"></div>
        <div class="d-flex gap-2 tag label">
          <div class="text-center">Fs {{ formatFrequency(props.unit.metadata.frequency) }}</div>
        </div>
        <div class="d-flex gap-2 tag label">
          <div class="text-center">Frame {{ props.unit.metadata.window }}</div>
        </div>
        <div class="d-flex gap-2 tag label">
          <div class="text-center">Pulse {{ props.unit.metadata.chirp }} us</div>
        </div>
        <div class="mx-2" style="width: 1px; height: 1.25rem; background-color: rgba(255,255,255,0.2);"></div>
        <div class="d-flex gap-2 tag label">
          <div class="text-center">{{ (props.unit.metadata.frequency / (props.unit.metadata.chirp)) }} samples/pulse</div>
        </div>
      </div>
    </div>

  </div>

  <div v-for="channel in props.unit.channels" :key="props.unit.channels.indexOf(channel)"
       class="d-flex gap-2 flex-column pt-1">
    <div class="d-flex gap-2" v-if="channel.spectrum">
<!--      -->
<!--      -->
      <OverWave :resample=false :values0="channel.signalI" :values1="channel.signalQ" :samples="props.unit.metadata.window" :name="`RX ${props.unit.channels.indexOf(channel)}`"></OverWave>
      <FFT :spectrum="channel.spectrum" name="ss" :lut="channel.frequencies" :frequencies="channel.peaks"></FFT>
<!--      3-->
<!--      -->
<!--      <Doppler :values0="channel.spectrum" :values1="[]" name=""></Doppler>-->
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
  <div>
<!--    <Polar style="width: 30rem" :pings="props.unit.phase" :t="props.unit.distance" delta="80" :fft="false" name=""></Polar>-->
<!--  <Doppler :values0="props.unit.phase.map(p => p).sort((a, b) => b-a)" :values1="[]" name=""></Doppler>-->
<!--  -->
<!--    <OverWave :fft="false" :values0="props.unit.phase" :values1="[]" name="Phase"></OverWave>-->
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