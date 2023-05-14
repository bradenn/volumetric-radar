<script lang="ts" setup>

import Beam from "@/components/Beam.vue";
import Header from "@/components/Header.vue";
import type {Zone} from "@/types";


const zones: Zone[] = [
  {
      name: "Bedroom",
      corners: [
          [0.00007071067811865477, 0.00007071067811865474],
          [2.0364675298172563, -2.036467529817257],
          [2.0364675298172563, -2.3764444702117493],
          [3.1364428386310497, -2.3764444702117498],
          [3.1364428386310506, 1.993616858877351],
          [0, 2.0199212311374914],
      ],
  },
    {
        name: "Classroom",
        corners: [{"x": 2, "y": 2}, {"x": 28, "y": 2}, {"x": 28, "y": 13}, {"x": 27, "y": 14}, {
            "x": 27,
            "y": 18
        }, {"x": 2, "y": 18}, {"x": 2, "y": 2}].map(d => [d.x / 3, d.y / 3]),
    },
    {
        name: "Living Room",
        corners: [
            [0.0001, 0], // Top Left
            [3.66, 0], // Top Right
            [3.66, 2.56],// Right Corner
            [3.2, 3.02], // Right Short wall
            [5.363, 5.183], // Right Long wall
            [4.224, 6.32127], // Front Door
            [3.707, 5.805], // Pantry Door Side
      [3.021, 6.491], // Pantry Face Side
      [3.537, 7.007], // Pantry Fridge Side
      [1.826, 8.7181], // Kitchen Far wall
      [-0.3374, 6.554], // Kitchen Left wall
      [0.285, 5.9317], // Kitchen Laundry Wall
      [-1.115, 4.5316], // Laundry Wall
      [0.3133, 3.103], // Room Wall
      [0.0001, 2.794], // Left Short Wall
    ],
  }
]

</script>

<template>
    <div class="d-flex gap-2 flex-column">
        <Header desc="Manage configured zones." name="Zones">
            <router-link class="element" to="/zones/create">Create Zone</router-link>
        </Header>
        <router-view></router-view>
        <div class="zone-grid">
            <div v-for="zone in zones">
                <Beam :pitch="-0.145893" :roll="2.435" :x="2.75" :y="2" :zone="zone" data="" unit="vRadar df15"></Beam>

            </div>
        </div>
    </div>
</template>

<style scoped>
.zone-grid {
    display: grid;
    grid-template-rows: repeat(2, minmax(4rem, 1fr));
    grid-template-columns: repeat(2, minmax(4rem, 1fr));
    grid-gap: 0.5rem;
}
</style>