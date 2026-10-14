<script setup lang="ts">
import Icon from './Icon.vue'

function act(action: 'minimize' | 'maximize' | 'close'): void {
  void window.api.windowAction(action)
}
</script>

<template>
  <div class="window-controls">
    <button aria-label="Minimize window" title="Minimize" @click="act('minimize')">
      <Icon name="minus" :size="16" />
    </button>
    <button
      aria-label="Maximize or restore window"
      title="Maximize or restore"
      @click="act('maximize')"
    >
      <Icon name="maximize" :size="13" />
    </button>
    <button class="window-close" aria-label="Close window" title="Close" @click="act('close')">
      <Icon name="close" :size="17" />
    </button>
  </div>
</template>
