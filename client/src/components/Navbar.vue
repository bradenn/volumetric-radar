<script setup lang="ts">


</script>

<template>
  <div class="navbar element">
    <div class="d-flex align-items-center gap-4">
      <div class="label-c1 label-o6 label-w500 lh-1">vRadar</div>
      <div class="d-flex align-items-center gap-3">
        <router-link class="navbar-router-link" to="/">Dashboard</router-link>
        <router-link class="navbar-router-link" to="/modules">Modules</router-link>
        <router-link class="navbar-router-link" to="/zones">Zones</router-link>
      </div>
    </div>
    <div class="d-flex gap-2 label-c3 label-w400 text-accent align-items-center">
      Braden<i class="fa-solid fa-caret-down"></i>
    </div>
  </div>
</template>

<style scoped>
.navbar {
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
  filter: drop-shadow(0px 10px 60px rgba(0, 0, 0, 0.1));
  transform: translate3d(0, 0, 0);
  background-color: hsla(214, 9%, 28%, 0.3);
  box-shadow: inset 0 0 2px 0.5px rgba(255, 255, 255, 0.125) !important;
  border-radius: 10px;
  padding: 12px 18px;
}
</style>