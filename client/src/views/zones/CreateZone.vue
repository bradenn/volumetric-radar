<script lang="ts" setup>

import Header from "@/components/Header.vue";
import ZonePreview from "@/views/zones/ZonePreview.vue";

</script>

<template>
  <Header name="Zones" desc="Create a zone">
  </Header>
  <div class="h-100">
  <ZonePreview></ZonePreview>
  </div>

</template>

<style scoped>
.zone-grid {
  display: grid;
  grid-template-rows: repeat(3, minmax(4rem, 1fr));
  grid-template-columns: repeat(3, minmax(4rem, 1fr));
  grid-gap: 0.5rem;
}
</style>