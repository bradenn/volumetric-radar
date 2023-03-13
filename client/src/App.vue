<script setup lang="ts">
import { RouterLink, RouterView } from "vue-router";
import Navbar from "@/components/Navbar.vue";
</script>

<template>
  <div class="mt-3 mx-2 " style="width: calc(100% - 1rem);">
    <Navbar></Navbar>
    <RouterView class="pt-2"/>
  </div>
</template>

<style scoped></style>
