<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue'
import type { WorkspaceMenu } from '@shared/workspace'

const props = defineProps<{ explorerVisible: boolean }>()
const emit = defineEmits<{ error: [message: string] }>()
const openedMenu = ref<WorkspaceMenu | null>(null)
let unsubscribe: (() => void) | undefined
const items: { id: WorkspaceMenu; label: string; key: string }[] = [
  { id: 'file', label: 'File', key: 'f' },
  { id: 'edit', label: 'Edit', key: 'e' },
  { id: 'view', label: 'View', key: 'v' },
  { id: 'help', label: 'Help', key: 'h' }
]

async function openMenu(menu: WorkspaceMenu): Promise<void> {
  if (openedMenu.value === menu) return
  const button = document.querySelector<HTMLElement>('[data-testid="menu-' + menu + '"]')
  if (!button) return
  const bounds = button.getBoundingClientRect()
  openedMenu.value = menu
  try {
    await window.api.openMenu({
      menu,
      x: bounds.x,
      y: bounds.bottom,
      explorerVisible: props.explorerVisible
    })
  } catch {
    emit('error', 'Could not open the menu. Please try again.')
  } finally {
    if (openedMenu.value === menu) openedMenu.value = null
  }
}

function keyboard(event: KeyboardEvent): void {
  if (event.altKey && !event.ctrlKey && !event.metaKey) {
    const item = items.find((item) => item.key === event.key.toLowerCase())
    if (item) {
      event.preventDefault()
      void openMenu(item.id)
    }
  }
}

function moveFocus(event: KeyboardEvent, index: number): void {
  let next: number
  if (event.key === 'ArrowRight') next = (index + 1) % items.length
  else if (event.key === 'ArrowLeft') next = (index - 1 + items.length) % items.length
  else if (event.key === 'Home') next = 0
  else if (event.key === 'End') next = items.length - 1
  else return
  event.preventDefault()
  document.querySelector<HTMLButtonElement>('[data-testid="menu-' + items[next].id + '"]')?.focus()
}

onMounted(() => {
  window.addEventListener('keydown', keyboard)
  unsubscribe = window.api.onMenuRequested((menu) => {
    void openMenu(menu)
  })
})
onUnmounted(() => {
  window.removeEventListener('keydown', keyboard)
  unsubscribe?.()
})
</script>

<template>
  <nav class="menu-bar" role="menubar" aria-label="Application menu">
    <button
      v-for="(item, index) in items"
      :key="item.id"
      role="menuitem"
      aria-haspopup="menu"
      :aria-expanded="openedMenu === item.id"
      :data-testid="'menu-' + item.id"
      :class="{ opened: openedMenu === item.id }"
      @mousedown.prevent
      @click="openMenu(item.id)"
      @keydown="moveFocus($event, index)"
    >
      {{ item.label }}
    </button>
  </nav>
</template>

<style scoped>
.menu-bar {
  display: flex;
  align-items: center;
  gap: 2px;
  padding-left: 12px;
  -webkit-app-region: no-drag;
}
.menu-bar button {
  padding: 3px 8px;
  border: 0;
  border-radius: 5px;
  background: transparent;
  color: var(--text);
  font-size: 13px;
  font-weight: 400;
  line-height: 1.4;
}
.menu-bar button:hover,
.menu-bar button.opened {
  background: var(--accent-soft);
  color: var(--accent);
}
</style>
