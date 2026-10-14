<script setup lang="ts">
import { computed, onUnmounted, ref } from 'vue'
import type { ApplicationInstance } from '@shared/workspace'
import Icon from './Icon.vue'
import WindowControls from './WindowControls.vue'

const props = defineProps<{ instance: ApplicationInstance }>()
const api = window.api
const docked = computed(() => props.instance.placement === 'docked')
const placementPending = ref(false)
const placementError = ref('')

async function changePlacement(): Promise<void> {
  placementPending.value = true
  placementError.value = ''
  try {
    await api.dockInstance(props.instance.id)
  } catch {
    placementError.value = 'Could not move this application. Please try again.'
  } finally {
    placementPending.value = false
  }
}
interface Frame {
  sequence: number
  time: string
  id: string
  name: string
  data: string
}
const messages = [
  { id: '0x123', name: 'Engine status' },
  { id: '0x245', name: 'Wheel speed' },
  { id: '0x301', name: 'Steering angle' },
  { id: '0x410', name: 'Body control' }
]
const frames = ref<Frame[]>([])
const generated = ref(0)
const running = ref(false)
const filter = ref('')
let timer: number | undefined
const visibleFrames = computed(() =>
  frames.value.filter((frame) =>
    [frame.id, frame.name, frame.data]
      .join(' ')
      .toLowerCase()
      .includes(filter.value.toLowerCase().trim())
  )
)

function emitFrame(): void {
  const sequence = ++generated.value
  const message = messages[(sequence - 1) % messages.length]
  const data = Array.from({ length: 8 }, (_, index) =>
    ((sequence * 17 + index * 29) % 256).toString(16).padStart(2, '0').toUpperCase()
  ).join(' ')
  const time =
    new Date().toLocaleTimeString('en-GB', { hour12: false }) +
    '.' +
    String(new Date().getMilliseconds()).padStart(3, '0')
  frames.value = [{ sequence, time, ...message, data }, ...frames.value].slice(0, 120)
}

function toggle(): void {
  if (running.value) {
    window.clearInterval(timer)
    timer = undefined
    running.value = false
  } else {
    running.value = true
    emitFrame()
    timer = window.setInterval(emitFrame, 500)
  }
}

function clear(): void {
  frames.value = []
  generated.value = 0
}

onUnmounted(() => window.clearInterval(timer))
</script>

<template>
  <div :class="['demo-window', { docked }]">
    <header v-if="!docked" class="captionbar demo-caption">
      <div class="demo-caption-label">
        <Icon name="activity" :size="16" /><strong>Bus Trace Demo</strong
        ><span class="caption-instance"
          >Instance {{ String(instance.ordinal).padStart(2, '0') }}</span
        >
      </div>
      <div class="demo-caption-actions">
        <button
          class="caption-action"
          data-testid="show-workspace"
          title="Show workspace"
          aria-label="Show workspace"
          @click="api.showWorkspace()"
        >
          <Icon name="home" :size="18" />
        </button>
        <button
          class="caption-action"
          :disabled="placementPending"
          data-testid="change-placement"
          title="Dock to Home"
          aria-label="Dock to Home"
          @click="changePlacement"
        >
          <Icon name="dock" :size="18" />
        </button>
        <WindowControls />
      </div>
    </header>
    <main class="demo-page">
      <p v-if="placementError" class="placement-error" role="alert">{{ placementError }}</p>
      <section class="trace-panel">
        <div class="trace-toolbar">
          <div class="trace-toolbar-title">
            <h2>Message trace</h2>
            <span class="count-pill">{{ visibleFrames.length }}</span>
          </div>
          <div class="trace-actions">
            <button
              class="button secondary"
              :disabled="!frames.length"
              data-testid="clear-trace"
              @click="clear"
            >
              <Icon name="reset" :size="15" />Clear</button
            ><button
              :class="['button', running ? 'secondary' : 'primary']"
              data-testid="toggle-stream"
              @click="toggle"
            >
              <Icon :name="running ? 'pause' : 'play'" :size="15" />{{
                running ? 'Pause' : generated ? 'Resume demo' : 'Start demo'
              }}
            </button>
          </div>
        </div>
        <div class="trace-filter">
          <Icon name="search" :size="16" /><input
            v-model="filter"
            placeholder="Filter by message ID, name, or payload..."
            aria-label="Filter trace messages"
            data-testid="trace-filter"
          /><span class="muted small">Newest first</span>
        </div>
        <div class="trace-table-scroll">
          <table class="trace-table">
            <thead>
              <tr>
                <th>#</th>
                <th>Time</th>
                <th>Message ID</th>
                <th>Name</th>
                <th>DLC</th>
                <th>Data</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="frame in visibleFrames" :key="frame.sequence" data-testid="trace-frame">
                <td class="frame-sequence">{{ frame.sequence }}</td>
                <td class="mono">{{ frame.time }}</td>
                <td class="frame-id mono">{{ frame.id }}</td>
                <td>{{ frame.name }}</td>
                <td class="mono">8</td>
                <td class="payload mono">{{ frame.data }}</td>
              </tr>
            </tbody>
          </table>
          <div v-if="!visibleFrames.length" class="trace-empty">
            <span class="trace-empty-icon"
              ><Icon :name="filter ? 'search' : 'activity'" :size="26" /></span
            ><strong>{{ filter ? 'No matching messages' : 'Your trace starts here' }}</strong>
            <p>
              {{
                filter
                  ? 'Try another message ID, name, or payload.'
                  : 'Select Start demo to begin streaming simulated CAN frames.'
              }}
            </p>
          </div>
        </div>
        <footer class="trace-footer">
          <span
            ><span :class="['tiny-dot', { streaming: running }]"></span
            >{{ running ? 'Simulation running' : 'Simulation idle' }}</span
          ><span>Classic CAN · 8-byte frames · Bounded buffer</span>
        </footer>
      </section>
    </main>
  </div>
</template>
