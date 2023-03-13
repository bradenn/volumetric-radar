<script setup lang="ts">

interface HeaderProps {
  name: string
  desc: string
}


const props = defineProps<HeaderProps>()



</script>

<template>
  <div class="header d-flex justify-content-between align-items-end mb-3">
    <div>
      <div class="label-cn2 label-w500 label-o6">{{props.name}}</div>
      <div class="label-c3 label-w400 label-o4 lh-1">{{props.desc}}</div>
    </div>
    <div>
      <slot></slot>
<!--      <div class="element"><i class="fa-solid fa-plus"></i> &nbsp;&nbsp;Add Module</div>-->
    </div>
  </div>

</template>

<style scoped>

</style>