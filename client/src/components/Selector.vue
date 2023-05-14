<script lang="ts" setup>

import {reactive} from "vue";
import {v4} from "uuid";


interface TagProps {
    name: string
    desc?: string
    unit?: string
    value: string
    active?: string
    border?: string
    disabled?: boolean
    options: any[]
    labels?: any[]
    change: (value: any) => void

}


const props = defineProps<TagProps>()

const state = reactive({
    toggled: false,
    id: v4(),
})

function touch(e: MouseEvent) {
    e.preventDefault()
    state.toggled = !state.toggled
    let target = document.getElementById(`${state.id}`) as HTMLDivElement

    target.style.display = state.toggled ? "grid" : "none";
}

function commitChange(e: MouseEvent, value: any) {
    e.preventDefault()
    if (props.change) {
        props.change(value)
    }
}

function leave(e: MouseEvent) {
    state.toggled = false
    let target = document.getElementById(`${state.id}`) as HTMLDivElement
    target.style.display = "none";
}

</script>

<template>
    <div class="dropdown" @click="touch">
        <div :class="`${props.disabled?'element-fg-disabled':''} `" class="element-fg d-flex gap-2 align-items-center">

            <div class="d-flex flex-column align-items-start ">

                <div class="label-c5 label-o4">
                    {{ props.name }}
                </div>
                <div :class="`${props.active} ${props.border?`tag-border-${props.border}`:''}`"
                     class="label-c4 label-o5 label-mono">
                    <slot></slot>
                </div>

            </div>
            <div class="caret">
                ⌵
            </div>

        </div>
        <div :id="state.id" class="dropdown-menu element" @mouseleave="leave">
            <div v-for="option in props.options"
                 :class="`${(`${props.value}` === `${option}`)?'element-fg':''}`"
                 class="dropdown-option font-monospace label-c4"
                 @click="(e) => commitChange(e, option)">
                <div v-if="props.labels">
                    {{ props.labels[props.options.indexOf(option)] }}
                </div>
                <div v-else>
                    {{ option }}{{ props.unit ? props.unit : '' }}
                </div>
            </div>
        </div>
    </div>
</template>

<style scoped>

.dropdown-option:hover {
    background-color: hsla(214, 9%, 28%, 0.25);
}

.dropdown-option.element-fg {
    padding: 12px 12px;
}

.dropdown-option {

    border-radius: 0.25rem;
    display: flex;
    justify-content: center;
    padding: 12px 12px;
}

.dropdown-option-active {
    background-color: rgba(19, 33, 23, 1) !important;
}

.element {
    display: flex;
    position: fixed;
    flex-direction: row;
    align-items: center;
    filter: drop-shadow(0px 10px 60px rgba(0, 0, 0, 0.1));
    /*transform: translate3d(0, 0, 0);*/
    background-color: rgba(19, 21, 23, 1) !important;
    color: rgba(255, 255, 255, 0.7);
    opacity: 1;

    z-index: 100 !important;
    box-shadow: inset 0 0 2px 0.5px rgba(255, 255, 255, 0.125) !important;
    border-radius: 8px;
    /*padding: 6px 12px;*/
}

.dropdown {
    position: relative;
    display: inline-block;
    z-index: 1200 !important;
    cursor: pointer;
}

@keyframes animateIn {
    0% {
        opacity: 0;
    }
    100% {
        opacity: 1;
    }
}

.dropdown-menu {

    margin-top: 0.25rem;
    display: none;
    grid-template-columns: repeat(4, minmax(2rem, 1fr));
    /*grid-template-rows: repeat(4, minmax(1rem, 1fr));*/
    flex-direction: row;
    flex-wrap: wrap;
    grid-column-gap: 0.25rem;
    grid-row-gap: 0.25rem;
    animation: animateIn 100ms ease-out;
    padding: 0.375rem;

}


.caret {
    padding-bottom: 0.3rem;
    color: rgba(255, 255, 255, 0.3)
}

.tag-border-danger {

    color: rgba(255, 69, 58, 0.2) !important;
}

.tag-border-critical {

    color: rgba(191, 90, 242, 0.6) !important;
}

.tag-border-warning {
    color: rgba(255, 214, 10, 0.8) !important;
}

.element-fg-disabled > div {
    filter: brightness(80%) contrast(10%);
    /*opacity: 0.3;*/
}
</style>