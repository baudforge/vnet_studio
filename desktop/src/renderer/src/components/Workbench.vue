<script setup lang="ts">
import { computed, nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import type { ApplicationInstance, WorkspaceState } from '@shared/workspace'
import { groupInstancesByCategory } from '../instance-groups'
import Icon from './Icon.vue'

const props = defineProps<{
  state: WorkspaceState
  explorerVisible: boolean
  collapsedCategories: string[]
}>()
const api = window.api
const emit = defineEmits<{
  browse: []
  error: [message: string]
  'update:collapsedCategories': [categories: string[]]
}>()
const host = ref<HTMLElement | null>(null)
const pending = ref(false)
const instanceCategories = computed(() => groupInstancesByCategory(props.state))
const dockedInstances = computed(() =>
  props.state.instances.filter((instance) => instance.placement === 'docked')
)
const detachedInstances = computed(() =>
  props.state.instances.filter((instance) => instance.placement === 'detached')
)
const active = computed(() =>
  dockedInstances.value.find((instance) => instance.id === props.state.activeInstanceId)
)
let observer: ResizeObserver | undefined
let mounted = false

function label(instance: ApplicationInstance): string {
  return instance.name + ' · ' + String(instance.ordinal).padStart(2, '0')
}

function toggleCategory(name: string): void {
  const categories = props.collapsedCategories.includes(name)
    ? props.collapsedCategories.filter((category) => category !== name)
    : [...props.collapsedCategories, name]
  emit('update:collapsedCategories', categories)
}

async function perform(action: () => Promise<unknown>): Promise<void> {
  if (pending.value) return
  pending.value = true
  try {
    await action()
  } catch {
    emit('error', 'The application action could not be completed. Please try again.')
  } finally {
    pending.value = false
  }
}

async function updateBounds(): Promise<void> {
  await nextTick()
  if (!mounted) return
  const bounds = active.value ? host.value?.getBoundingClientRect() : null
  try {
    await api.setDockBounds(
      bounds
        ? {
            x: bounds.x,
            y: bounds.y,
            width: bounds.width,
            height: bounds.height
          }
        : null
    )
  } catch {
    if (mounted) emit('error', 'Could not display the application page.')
  }
}

function tabKey(event: KeyboardEvent, instance: ApplicationInstance): void {
  const instances = dockedInstances.value
  const current = instances.findIndex((entry) => entry.id === instance.id)
  let index: number
  if (event.key === 'ArrowRight') index = (current + 1) % instances.length
  else if (event.key === 'ArrowLeft') index = (current - 1 + instances.length) % instances.length
  else if (event.key === 'Home') index = 0
  else if (event.key === 'End') index = instances.length - 1
  else return
  event.preventDefault()
  const next = instances[index]
  void perform(async () => {
    await api.focusInstance(next.id)
    await nextTick()
    document.getElementById('tab-' + next.id)?.focus()
  })
}

watch(
  () => [props.state.activeInstanceId, dockedInstances.value.length, props.explorerVisible],
  () => void updateBounds(),
  { flush: 'post' }
)
onMounted(() => {
  mounted = true
  observer = new ResizeObserver(() => void updateBounds())
  if (host.value) observer.observe(host.value)
  void updateBounds()
})
onUnmounted(() => {
  mounted = false
  observer?.disconnect()
  void api.setDockBounds(null).catch(() => {})
})
</script>

<template>
  <section class="workbench" data-testid="home-page">
    <aside
      v-if="explorerVisible"
      id="home-instance-list"
      class="instance-explorer"
      aria-label="Application instances"
      data-testid="instance-explorer"
    >
      <header class="explorer-heading">
        <span>APPLICATIONS</span>
        <button
          class="icon-button"
          title="Launch an application"
          aria-label="Launch an application"
          @click="emit('browse')"
        >
          <Icon name="plus" :size="17" />
        </button>
      </header>
      <div class="explorer-list">
        <p v-if="!state.instances.length" class="explorer-empty">No running applications</p>
        <section
          v-for="category in instanceCategories"
          :key="category.name"
          class="instance-category"
          :data-category="category.name"
        >
          <button
            class="category-toggle"
            data-testid="category-toggle"
            :aria-expanded="!collapsedCategories.includes(category.name)"
            :aria-controls="category.id"
            @click="toggleCategory(category.name)"
          >
            <Icon
              name="chevron"
              :size="13"
              :class="{ expanded: !collapsedCategories.includes(category.name) }"
            />
            <span>{{ category.name }}</span>
            <span class="count-pill">{{ category.instances.length }}</span>
          </button>
          <div
            v-if="!collapsedCategories.includes(category.name)"
            :id="category.id"
            class="category-instances"
          >
            <div
              v-for="instance in category.instances"
              :key="instance.id"
              :data-instance-id="instance.id"
              :class="['explorer-instance', { active: instance.id === state.activeInstanceId }]"
            >
              <button
                class="explorer-select"
                data-testid="focus-instance"
                :title="
                  label(instance) + (instance.placement === 'detached' ? ' (external window)' : '')
                "
                :aria-label="'Open ' + label(instance)"
                :disabled="pending"
                @click="perform(() => api.focusInstance(instance.id))"
              >
                <Icon
                  :name="instance.placement === 'detached' ? 'external' : 'activity'"
                  :size="15"
                />
                <span
                  >{{ instance.name
                  }}<small>{{ String(instance.ordinal).padStart(2, '0') }}</small></span
                >
              </button>
              <button
                v-if="instance.placement === 'detached'"
                class="icon-button explorer-action"
                data-testid="dock-instance"
                :title="'Dock ' + label(instance) + ' to Home'"
                :aria-label="'Dock ' + label(instance) + ' to Home'"
                :disabled="pending"
                @click="perform(() => api.dockInstance(instance.id))"
              >
                <Icon name="dock" :size="14" />
              </button>
              <button
                class="icon-button explorer-action"
                data-testid="close-instance"
                :title="'Close ' + label(instance)"
                :aria-label="'Close ' + label(instance)"
                :disabled="pending"
                @click="perform(() => api.closeInstance(instance.id))"
              >
                <Icon name="close" :size="14" />
              </button>
            </div>
          </div>
        </section>
      </div>
    </aside>
    <div class="editor-group">
      <div class="editor-header">
        <div class="editor-tabs" role="tablist" aria-label="Docked application pages">
          <div
            v-for="instance in dockedInstances"
            :key="instance.id"
            :class="['editor-tab', { active: instance.id === state.activeInstanceId }]"
            :data-tab-instance="instance.id"
          >
            <button
              :id="'tab-' + instance.id"
              role="tab"
              :aria-selected="instance.id === state.activeInstanceId"
              aria-controls="app-editor"
              :tabindex="instance.id === state.activeInstanceId ? 0 : -1"
              :title="label(instance)"
              :disabled="pending"
              @click="perform(() => api.focusInstance(instance.id))"
              @keydown="tabKey($event, instance)"
            >
              <Icon name="activity" :size="15" /><span>{{ instance.name }}</span
              ><small>{{ String(instance.ordinal).padStart(2, '0') }}</small>
            </button>
            <button
              class="tab-close"
              :aria-label="'Close ' + label(instance)"
              :title="'Close ' + label(instance)"
              :disabled="pending"
              data-testid="close-tab"
              @click="perform(() => api.closeInstance(instance.id))"
            >
              <Icon name="close" :size="14" />
            </button>
          </div>
          <span v-if="!dockedInstances.length" class="welcome-tab"
            ><Icon name="home" :size="15" />Welcome</span
          >
        </div>
        <div class="editor-actions">
          <button
            v-if="active"
            class="icon-button"
            data-testid="detach-active"
            title="Move active application to a window"
            aria-label="Move active application to a window"
            :disabled="pending"
            @click="perform(() => api.detachInstance(active!.id))"
          >
            <Icon name="external" :size="16" />
          </button>
          <button
            class="icon-button"
            title="Launch an application"
            aria-label="Launch an application"
            @click="emit('browse')"
          >
            <Icon name="plus" :size="17" />
          </button>
        </div>
      </div>
      <div
        id="app-editor"
        ref="host"
        class="editor-host"
        role="tabpanel"
        :aria-labelledby="active ? 'tab-' + active.id : undefined"
        data-testid="editor-host"
      >
        <div v-if="!active" class="editor-welcome">
          <div class="welcome-mark"><Icon name="activity" :size="58" /></div>
          <h1>VNet Studio</h1>
          <p>Your applications. Your workspace.</p>
          <button class="button primary" data-testid="browse-apps" @click="emit('browse')">
            <Icon name="plus" :size="16" />Open an application
          </button>
          <div class="welcome-tips">
            <span>Launch from Apps</span><span>Work in tabs</span><span>Move to a window</span>
          </div>
          <p v-if="detachedInstances.length" class="external-hint">
            {{ detachedInstances.length }} application{{
              detachedInstances.length === 1 ? '' : 's'
            }}
            in external windows. Dock them from the instance list.
          </p>
        </div>
      </div>
      <footer class="editor-status">
        <span
          ><span class="status-dot"></span
          >{{ active?.status === 'starting' ? 'Opening application' : 'Workspace ready' }}</span
        ><span
          >{{ dockedInstances.length }} docked<span class="status-separator"
            >{{ detachedInstances.length }} external</span
          ><span v-if="active" class="status-separator">{{ label(active) }}</span></span
        >
      </footer>
    </div>
  </section>
</template>

<style scoped>
.icon-button {
  padding: 0;
  border: 0;
  border-radius: 4px;
}
.icon-button:hover {
  color: var(--text);
  background: var(--border);
}
.workbench {
  display: flex;
  flex: 1;
  min-height: 0;
}
.instance-explorer {
  width: 260px;
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  background: var(--surface-soft);
  border-right: 1px solid var(--border);
}
.explorer-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 40px;
  padding: 0 13px 0 18px;
  font-size: 13px;
  color: var(--muted);
  letter-spacing: 1px;
  border-bottom: 1px solid var(--border);
}
.explorer-heading .icon-button {
  width: 24px;
  height: 24px;
}
.category-toggle {
  display: flex;
  align-items: center;
  gap: 7px;
  width: 100%;
  height: 32px;
  padding: 0 12px;
  border: 0;
  background: transparent;
  color: var(--text);
  text-align: left;
  font-size: 13px;
  font-weight: 500;
}
.category-toggle:hover {
  background: var(--surface);
}
.category-toggle > svg.expanded {
  transform: rotate(90deg);
}
.category-toggle .count-pill {
  margin-left: auto;
  background: var(--surface);
}
.instance-category + .instance-category {
  margin-top: 5px;
}
.explorer-empty {
  color: var(--muted);
  font-size: 13px;
  padding: 16px 18px;
}
.explorer-list {
  overflow: auto;
  flex: 1;
}
.explorer-instance {
  display: flex;
  align-items: center;
  height: 34px;
  padding-right: 5px;
}
.explorer-instance:hover {
  background: var(--surface);
}
.explorer-instance.active {
  background: var(--accent-soft);
}
.explorer-select {
  flex: 1;
  min-width: 0;
  display: flex;
  align-items: center;
  gap: 9px;
  height: 100%;
  padding: 0 5px 0 28px;
  border: 0;
  background: transparent;
  color: var(--text);
  font-size: 13px;
  font-weight: 400;
  text-align: left;
}
.explorer-select > svg {
  color: var(--accent);
}
.explorer-select > span {
  display: flex;
  align-items: center;
  gap: 5px;
  min-width: 0;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.explorer-select small {
  font-size: 12px;
  color: var(--muted);
}
.explorer-action {
  width: 22px;
  height: 24px;
  flex-shrink: 0;
}
.editor-group {
  display: flex;
  flex: 1;
  min-width: 0;
  min-height: 0;
  flex-direction: column;
}
.editor-header {
  display: flex;
  flex-shrink: 0;
  height: 40px;
  background: var(--surface-soft);
  border-bottom: 1px solid var(--border);
}
.editor-tabs {
  display: flex;
  flex: 1;
  min-width: 0;
  overflow-x: auto;
  scrollbar-width: thin;
}
.editor-tab {
  position: relative;
  display: flex;
  align-items: center;
  flex-shrink: 0;
  border-right: 1px solid var(--border);
  color: var(--muted);
}
.editor-tab.active {
  background: var(--background);
  color: var(--text);
}
.editor-tab.active::before {
  content: '';
  position: absolute;
  left: 0;
  right: 0;
  top: 0;
  height: 2px;
  background: var(--accent);
}
.editor-tab > button[role='tab'] {
  display: flex;
  align-items: center;
  gap: 8px;
  height: 100%;
  border: 0;
  padding: 0 10px 0 15px;
  font-size: 13px;
  font-weight: 400;
  color: inherit;
  background: transparent;
}
.editor-tab svg {
  color: var(--accent);
}
.editor-tab small {
  font-size: 12px;
  color: var(--muted);
}
.tab-close {
  width: 22px;
  height: 24px;
  display: grid;
  place-items: center;
  padding: 0;
  margin-right: 6px;
  border: 0;
  border-radius: 4px;
  background: transparent;
}
.tab-close svg {
  color: var(--muted);
}
.tab-close:hover {
  background: var(--border);
}
.welcome-tab {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 0 20px;
  background: var(--background);
  font-size: 13px;
  color: var(--muted);
  border-right: 1px solid var(--border);
}
.editor-actions {
  display: flex;
  align-items: center;
  gap: 2px;
  padding: 0 9px;
}
.editor-actions .icon-button {
  width: 24px;
  height: 24px;
}
.editor-host {
  flex: 1;
  min-height: 0;
  position: relative;
}
.editor-welcome {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 16px;
  height: 100%;
  padding: 30px;
  text-align: center;
}
.welcome-mark {
  display: grid;
  place-items: center;
  width: 104px;
  height: 104px;
  margin-bottom: 8px;
  border-radius: 28px;
  background: var(--accent-soft);
  color: var(--accent);
}
.editor-welcome h1 {
  font-size: 24px;
  letter-spacing: -0.8px;
  font-weight: 600;
}
.editor-welcome > p {
  color: var(--muted);
  font-size: 13px;
}
.editor-welcome .button {
  margin-top: 10px;
}
.welcome-tips {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  justify-content: center;
  gap: 17px;
  margin-top: 22px;
  color: var(--muted);
  font-size: 13px;
}
.welcome-tips > span + span::before {
  content: '·';
  margin-right: 17px;
}
.editor-welcome > p.external-hint {
  font-size: 13px;
  line-height: 1.8;
  max-width: 370px;
}
.editor-status {
  display: flex;
  flex-shrink: 0;
  align-items: center;
  justify-content: space-between;
  height: 28px;
  padding: 0 13px;
  gap: 10px;
  font-size: 12px;
  color: var(--muted);
  background: var(--surface);
  border-top: 1px solid var(--border);
}
.editor-status > span {
  display: flex;
  align-items: center;
  gap: 7px;
  white-space: nowrap;
}
.status-separator {
  margin-left: 12px;
}
@media (max-width: 1100px) {
  .instance-explorer {
    width: 240px;
  }
  .explorer-select {
    padding-left: 24px;
    gap: 7px;
  }
  .editor-status .status-separator:last-child {
    display: none;
  }
}
</style>
