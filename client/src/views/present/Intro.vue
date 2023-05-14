<script lang="ts" setup>

interface Metadata {
    frame: number[]
}

</script>

<template>
    <div class="page-grid">
        Page one
    </div>
</template>

<style scoped>
.page-grid {
    display: grid;
    grid-template-columns: repeat(12, minmax(20rem, 1fr));
    gap: 0.5rem;
}

.tool-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(20rem, 1fr));
    gap: 0.5rem;
}
</style>
