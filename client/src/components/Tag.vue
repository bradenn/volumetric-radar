<script lang="ts" setup>

interface TagProps {
  name: string
  desc?: string
  value: string
  active?: string
  disabled?: boolean
}


const props = defineProps<TagProps>()

</script>

<template>
  <div :class="props.disabled?'element-fg-disabled':''" class="element-fg">
    <div class="d-flex flex-column align-items-start ">
      <div class="label-c5 label-o4">
        {{ props.name }}
      </div>
      <div :class="props.active" class="label-c4 label-o5 label-mono">
        {{ props.value }}
      </div>

    </div>

  </div>

</template>

<style scoped>
.element-fg-disabled > div {
  filter: brightness(80%) contrast(10%);
  /*opacity: 0.3;*/
}
</style>