<script setup lang="ts">
import { computed, defineAsyncComponent, onMounted, onUnmounted, ref, watchEffect } from 'vue'
import type { Bootstrap, WorkspaceState } from '@shared/workspace'
const Launcher = defineAsyncComponent(() => import('./components/Launcher.vue'))

const bootstrap = ref<Bootstrap | null>(null)
const state = ref<WorkspaceState | null>(null)
const failure = ref(false)
const applicationViews = {
  'bus-trace-demo': defineAsyncComponent(() => import('./components/DemoApp.vue'))
}
const applicationView = computed(() => {
  const context = bootstrap.value?.context
  return context?.kind === 'application'
    ? applicationViews[context.instance.applicationId as keyof typeof applicationViews]
    : undefined
})
const applicationInstance = computed(() => {
  const context = bootstrap.value?.context
  if (context?.kind !== 'application') return undefined
  return (
    state.value?.instances.find((instance) => instance.id === context.instance.id) ??
    context.instance
  )
})
const darkSystem = ref(window.matchMedia('(prefers-color-scheme: dark)').matches)
const media = window.matchMedia('(prefers-color-scheme: dark)')
let unsubscribe: (() => void) | undefined

function applyState(next: WorkspaceState): void {
  if (!state.value || next.revision >= state.value.revision) state.value = next
}

function updateSystemTheme(event: MediaQueryListEvent): void {
  darkSystem.value = event.matches
}

watchEffect(() => {
  const context = bootstrap.value?.context
  document.title =
    context?.kind === 'application'
      ? context.instance.name + ' · Instance ' + context.instance.ordinal + ' · VNet Studio'
      : 'VNet Studio'
  const theme = state.value?.settings.theme ?? 'light'
  document.documentElement.dataset.theme =
    theme === 'system' ? (darkSystem.value ? 'dark' : 'light') : theme
})

onMounted(async () => {
  media.addEventListener('change', updateSystemTheme)
  unsubscribe = window.api.onStateChanged(applyState)
  try {
    const initial = await window.api.getBootstrap()
    bootstrap.value = initial
    applyState(initial.state)
  } catch {
    failure.value = true
  }
})

onUnmounted(() => {
  unsubscribe?.()
  media.removeEventListener('change', updateSystemTheme)
})
</script>

<template>
  <Launcher v-if="bootstrap?.context.kind === 'workspace' && state" :state="state" />
  <component
    :is="applicationView"
    v-else-if="applicationInstance && applicationView"
    :instance="applicationInstance"
  />
  <div v-else class="boot-screen">
    <div class="boot-progress"></div>
    <strong>{{ failure ? 'Unable to open the workspace' : 'Opening VNet Studio' }}</strong>
    <p v-if="failure">Close this window and try launching the application again.</p>
    <span v-else class="boot-progress"></span>
  </div>
</template>
