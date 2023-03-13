<script lang="ts" setup>

import Module from "@/components/Module.vue";

import type {Device} from "@/types";
import Header from "@/components/Header.vue";

const samples: Device[] = [
  {
    name: "Living Room",
    model: "K24-R2",
    firmware: "1.0.2",
    connected: true
  },
  {
    name: "Bedroom",
    model: "K24-R2",
    firmware: "1.0.2",
    connected: true
  },
  {
    name: "Patio",
    model: "K24-R2",
    firmware: "1.0.2",
    connected: false
  }
] as Device[]

</script>

<template>
  <Header name="Modules" desc="Create and manage module devices.">
    <div class="element">
      Add Module
    </div>
  </Header>
  <div class="module-grid gap-2">
    <Module v-for="module in samples" :device="module"></Module>

  </div>
</template>

<style scoped>
.module-grid {
  display: grid;

  grid-template-columns: repeat(3, minmax(4rem, 1fr));
  grid-template-rows: repeat(3, minmax(4rem, 1fr));

}
</style>
