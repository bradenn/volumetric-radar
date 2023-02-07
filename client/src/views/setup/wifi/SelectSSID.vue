<script setup lang="ts">

const devices = [{
  ssid: "udap",
  rssi: -22,
},{
  ssid: "FBI Van 55",
  rssi: -38,
},{
  ssid: "Coffee Shop",
  rssi: -68,
},{
  ssid: "Pizza Store",
  rssi: -80,
}]

const signalRange = [{
  name: "Strong",
  begin: -30
},{
  name: "Good",
  begin: -55
},{
  name: "Acceptable",
  begin: -67
}, {
  name: "Poor",
  begin: -90
}]

</script>

<template>
  <h3>Select a secured network</h3>
  <div class="menu">
    <router-link :to="`/setup/wifi/${device.ssid}`" v-for="device in devices" class="menu-bar">
      <div class="menu-option">
        <div>{{device.ssid}}</div>
        <div>{{signalRange.find(s => s.begin <= device.rssi).name}} ({{device.rssi}} dB)</div>
      </div>
      <i class="gg-chevron-right" style='opacity:0.8;'></i>
    </router-link>
  </div>
</template>

<style>

</style>
