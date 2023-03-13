<script setup lang="ts">


import type {Device} from "@/types";
import moment from "moment";

interface ModuleProps {
  device: Device
}

const props = defineProps<ModuleProps>()

function timeSince(): string {
  return moment(props.device.lastSeen).subtract(1000*60*203).fromNow()
}

</script>

<template>
  <div class="element module gap-3 p-0">
    <div class="d-flex align-items-center gap-3 px-3 py-1 w-100">
      <div class="thumbnail" :style="`background-image: url('./vradar-k24-r2.png')`"></div>
      <div class="d-flex justify-content-between">
        <div>
          <div class="label-w500 label-c3">
            {{ props.device.name }}
          </div>
          <div class="label-w400 label-c4 label-o4">
            {{ props.device.model }}
          </div>
        </div>

      </div>
    </div>
    <div class="d-flex gap-1 align-items-center">
    <div class="d-flex flex-column align-items-end ">
      <div v-if="props.device.connected" class="label-c3 label-o5 label-w500 text-success">Connected</div>
      <div v-else class="label-c3 label-o5 label-w500 text-danger">Unreachable</div>
      <div v-if="props.device.connected" class="label-w400 label-c4 label-o4">
        v{{ props.device.firmware }}
      </div>
      <div v-else class="label-w400 label-c4 label-o4">
          {{timeSince()}}
      </div>
    </div>
    <div class="element button"><i class="fa-solid fa-gear"></i></div>
    </div>
  </div>
</template>

<style scoped>
.button {
  border-radius: 6px;
  height:1.5rem;
  margin: 8px 8px;
}
.thumbnail {
  width: 3.5rem;
  background-position: center;
  background-size: cover;
  aspect-ratio: 1/0.7;
  background-repeat: no-repeat;

  /*border: 1px solid white;*/
}

.module {
  display: flex;
  align-items: center;
  padding: 8px 16px;
  border-radius: 12px !important;
}
</style>