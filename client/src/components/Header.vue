<script setup lang="ts">

interface HeaderProps {
  name: string
  desc?: string
}


const props = defineProps<HeaderProps>()


</script>

<template>
  <div class="element p-2">
    <div class="d-flex gap-1 align-items-center gap-1">
      <div class="label-w500 labe-c3 lh-1 px-2" style="width: 5.75rem">{{ props.name }}</div>
      <div class="px-2"></div>
      <slot></slot>
    </div>

  </div>

</template>

<style scoped>

</style>