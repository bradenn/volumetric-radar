<script lang="ts" setup>

interface TagProps {
  name: string
  desc?: string
  value: string
  active?: string
  border?: string
  disabled?: boolean

}


const props = defineProps<TagProps>()

</script>

<template>
  <div :class="`${props.disabled?'element-fg-disabled':''} `" class="element-fg">
    <div class="d-flex flex-column align-items-start ">
      <div class="label-c5 label-o4" >
        {{ props.name }}
      </div>
      <div :class="`${props.active} ${props.border?`tag-border-${props.border}`:''}`" class="label-c4 label-o5 label-mono">
        {{ props.value }}
      </div>

    </div>

  </div>

</template>

<style scoped>
.tag-border-danger {

 color: rgba(255,69,58,0.2) !important;
}

.tag-border-critical {

    color: rgba(191,90,242,0.6) !important;
}

.tag-border-warning {
    color: rgba(255,214, 10,0.8) !important;
}

.element-fg-disabled > div {
  filter: brightness(80%) contrast(10%);
  /*opacity: 0.3;*/
}
</style>