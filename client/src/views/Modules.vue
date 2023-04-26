<script lang="ts" setup>

import Module from "@/components/Module.vue";

import type {Device} from "@/types";
import Header from "@/components/Header.vue";

const samples: Device[] = [
    {
        name: "Living Room",
        model: "VRAD-K24-R4",
        firmware: "2.0.2",
        connected: true
    },
    {
        name: "Bedroom",
        model: "VRAD-K24-R4",
        firmware: "2.0.2",
        connected: true
    },
    {
        name: "Patio",
        model: "VRAD-K24-R2",
        firmware: "1.1.2",
        connected: false
    }
] as Device[]

</script>

<template>
    <div>
        <Header desc="Create and manage module devices." name="Modules">
            <div class="element">
                Add Module
            </div>
        </Header>
        <div class="module-grid gap-2 mt-2">
            <Module v-for="module in samples" :device="module"></Module>

        </div>
    </div>
</template>

<style scoped>
.module-grid {
    display: grid;

    grid-template-columns: repeat(3, minmax(4rem, 1fr));
    grid-template-rows: repeat(3, minmax(4rem, 1fr));

}
</style>
