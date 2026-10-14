<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, shallowRef } from 'vue'
import type { LogLevel, LogSnapshot } from '@shared/workspace'
import Icon from './Icon.vue'

const emit = defineEmits<{ error: [message: string] }>()
const logs = shallowRef<LogSnapshot>({ revision: -1, limit: 500, entries: [] })
const query = ref('')
const level = ref<LogLevel | ''>('')
const loading = ref(true)
const failed = ref(false)
const clearing = ref(false)
let unsubscribe: (() => void) | undefined
let disposed = false

const visibleEntries = computed(() => {
  const search = query.value.trim().toLowerCase()
  return logs.value.entries.filter(
    (entry) =>
      (!level.value || entry.level === level.value) &&
      (!search || `${entry.source} ${entry.message}`.toLowerCase().includes(search))
  )
})

function update(snapshot: LogSnapshot): void {
  if (disposed || snapshot.revision < logs.value.revision) return
  logs.value = snapshot
  loading.value = false
  failed.value = false
}

async function load(): Promise<void> {
  loading.value = true
  failed.value = false
  try {
    update(await window.api.getLogs())
  } catch {
    if (!disposed) {
      failed.value = true
      loading.value = false
    }
  }
}

async function clear(): Promise<void> {
  if (clearing.value) return
  clearing.value = true
  try {
    await window.api.clearLogs()
  } catch {
    emit('error', 'Could not clear the logs. Please try again.')
  } finally {
    clearing.value = false
  }
}

function time(timestamp: string): string {
  return (
    new Date(timestamp).toLocaleTimeString(undefined, { hour12: false }) +
    '.' +
    timestamp.slice(20, 23)
  )
}

onMounted(() => {
  unsubscribe = window.api.onLogsChanged(update)
  void load()
})
onUnmounted(() => {
  disposed = true
  unsubscribe?.()
})
</script>

<template>
  <section class="log-viewer" data-testid="logs-page" aria-label="Runtime logs">
    <header class="log-toolbar">
      <h1>Logs</h1>
      <span class="log-count">{{ visibleEntries.length }}</span>
      <label class="log-search">
        <Icon name="search" :size="15" />
        <input
          v-model="query"
          aria-label="Search logs"
          placeholder="Search logs..."
          data-testid="log-search"
        />
      </label>
      <select v-model="level" aria-label="Log level" data-testid="log-level">
        <option value="">All levels</option>
        <option value="info">Info</option>
        <option value="warning">Warning</option>
        <option value="error">Error</option>
      </select>
      <button
        class="button secondary"
        :disabled="clearing || !logs.entries.length"
        data-testid="clear-logs"
        @click="clear"
      >
        <Icon name="reset" :size="14" />Clear
      </button>
    </header>
    <div class="log-scroll">
      <table class="log-table" aria-label="Log entries">
        <thead>
          <tr>
            <th scope="col">Time</th>
            <th scope="col">Level</th>
            <th scope="col">Source</th>
            <th scope="col">Message</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="entry in visibleEntries" :key="entry.id" data-testid="log-entry">
            <td class="log-time" :title="entry.timestamp">{{ time(entry.timestamp) }}</td>
            <td>
              <span :class="['log-level', entry.level]">{{ entry.level }}</span>
            </td>
            <td class="log-source">{{ entry.source }}</td>
            <td class="log-message">{{ entry.message }}</td>
          </tr>
        </tbody>
      </table>
      <div v-if="loading || failed || !visibleEntries.length" class="log-empty" role="status">
        <template v-if="failed">
          <span>Could not load the logs.</span>
          <button class="button secondary" data-testid="retry-logs" @click="load">Retry</button>
        </template>
        <span v-else-if="loading">Loading logs...</span>
        <span v-else>{{
          logs.entries.length ? 'No matching log entries.' : 'No log entries.'
        }}</span>
      </div>
    </div>
    <footer class="log-footer">
      <span>Current session</span>
      <span>{{ logs.entries.length }} / {{ logs.limit }} entries · Oldest first</span>
    </footer>
  </section>
</template>

<style scoped>
.log-viewer {
  display: flex;
  flex: 1;
  min-height: 0;
  flex-direction: column;
  background: var(--background);
}
.log-toolbar {
  display: flex;
  flex-shrink: 0;
  align-items: center;
  gap: 10px;
  padding: 10px 16px;
  border-bottom: 1px solid var(--border);
  background: var(--surface);
}
.log-toolbar h1 {
  font-size: 13px;
  font-weight: 500;
  letter-spacing: 0;
}
.log-count {
  color: var(--muted);
  font-size: 12px;
}
.log-search {
  display: flex;
  flex: 1;
  min-width: 0;
  align-items: center;
  gap: 7px;
  margin-left: 12px;
  color: var(--muted);
}
.log-search input {
  width: 100%;
  min-width: 0;
  padding: 5px 0;
  border: 0;
  background: transparent;
  color: var(--text);
  font-size: 13px;
}
.log-toolbar select {
  padding: 5px 8px;
  border: 1px solid var(--border);
  border-radius: 5px;
  background: var(--surface);
  color: var(--text);
  font: inherit;
}
.log-toolbar .button {
  padding: 5px 9px;
}
.log-scroll {
  flex: 1;
  min-height: 0;
  overflow: auto;
}
.log-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
  text-align: left;
}
.log-table th {
  position: sticky;
  top: 0;
  padding: 9px 16px;
  background: var(--surface-soft);
  border-bottom: 1px solid var(--border);
  color: var(--muted);
  font-weight: 500;
}
.log-table td {
  padding: 8px 16px;
  border-bottom: 1px solid var(--border);
  vertical-align: top;
}
.log-table tr:hover td {
  background: var(--surface-soft);
}
.log-time,
.log-source,
.log-table td:nth-child(2) {
  width: 1%;
  white-space: nowrap;
}
.log-time {
  font-family: 'Cascadia Code', Consolas, monospace;
  color: var(--muted);
}
.log-level {
  text-transform: capitalize;
  color: var(--muted);
}
.log-level.warning {
  color: var(--warning);
}
.log-level.error {
  color: var(--error);
}
.log-message {
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
.log-empty {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 12px;
  padding: 48px 16px;
  color: var(--muted);
}
.log-footer {
  display: flex;
  flex-shrink: 0;
  justify-content: space-between;
  gap: 10px;
  padding: 6px 16px;
  border-top: 1px solid var(--border);
  background: var(--surface);
  color: var(--muted);
  font-size: 12px;
}
</style>
