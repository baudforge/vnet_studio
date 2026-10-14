<script setup lang="ts">
import { computed, defineAsyncComponent, onMounted, onUnmounted, ref } from 'vue'
import type { ApplicationDefinition, Theme, WorkspacePage, WorkspaceState } from '@shared/workspace'
import Icon from './Icon.vue'
import WindowControls from './WindowControls.vue'
import Workbench from './Workbench.vue'
import MenuBar from './MenuBar.vue'
const DocsReader = defineAsyncComponent(() => import('./HelpReader.vue'))
const LogViewer = defineAsyncComponent(() => import('./LogViewer.vue'))

const props = defineProps<{ state: WorkspaceState }>()
const api = window.api
const page = ref<WorkspacePage>('home')
const explorerVisible = ref(true)
const collapsedCategories = ref<string[]>([])
const docsChapter = ref('workspace/getting-started')
const query = ref('')
const pending = ref('')
const toast = ref('')
let toastTimer: number | undefined
let unsubscribeActivation: (() => void) | undefined
let unsubscribeNavigation: (() => void) | undefined
let unsubscribeExplorer: (() => void) | undefined
const navigation = [
  { id: 'home' as const, label: 'Home', icon: 'home', shortcut: '1' },
  { id: 'logs' as const, label: 'Logs', icon: 'logs', shortcut: '2' },
  { id: 'apps' as const, label: 'Apps', icon: 'apps', shortcut: '3' },
  { id: 'docs' as const, label: 'Docs', icon: 'document', shortcut: '4' }
]
const themes: { value: Theme; label: string; icon: string }[] = [
  { value: 'light', label: 'Light', icon: 'sun' },
  { value: 'dark', label: 'Dark', icon: 'moon' },
  { value: 'system', label: 'System', icon: 'monitor' }
]
const filteredApps = computed(() =>
  props.state.applications.filter((application) =>
    [application.name, application.description, ...application.tags]
      .join(' ')
      .toLowerCase()
      .includes(query.value.toLowerCase().trim())
  )
)

function notify(message: string): void {
  toast.value = message
  window.clearTimeout(toastTimer)
  toastTimer = window.setTimeout(() => {
    toast.value = ''
  }, 3500)
}

async function perform(
  key: string,
  action: () => Promise<unknown>,
  success?: string
): Promise<void> {
  if (pending.value) return
  pending.value = key
  try {
    await action()
    if (success) notify(success)
  } catch {
    notify('The action could not be completed. Please try again.')
  } finally {
    pending.value = ''
  }
}

function launch(application: ApplicationDefinition): void {
  void perform('launch:' + application.id, () => window.api.launchApplication(application.id))
}

async function setEnabled(application: ApplicationDefinition, event: Event): Promise<void> {
  const input = event.target as HTMLInputElement
  await perform('enabled', () => window.api.setApplicationEnabled(application.id, input.checked))
  input.checked =
    props.state.applications.find((entry) => entry.id === application.id)?.enabled ??
    application.enabled
}

function chooseTheme(theme: Theme): void {
  void perform('settings', () => window.api.updateSettings({ theme }), 'Preferences saved')
}

function changeExitPreference(event: Event): void {
  const closeAppsOnExit = (event.target as HTMLInputElement).checked
  void perform(
    'settings',
    () => window.api.updateSettings({ closeAppsOnExit }),
    'Preferences saved'
  )
}

function openCount(applicationId: string): number {
  return props.state.instances.filter((instance) => instance.applicationId === applicationId).length
}

function navigate(destination: WorkspacePage): void {
  if (destination === 'home' && page.value === 'home') {
    explorerVisible.value = !explorerVisible.value
  } else {
    page.value = destination
  }
}

function shortcut(event: KeyboardEvent): void {
  const target = event.target
  if (target instanceof HTMLElement && target.closest('input, textarea, [contenteditable="true"]'))
    return
  if (event.key === '/' && !event.ctrlKey && !event.metaKey && page.value === 'apps') {
    event.preventDefault()
    document.querySelector<HTMLInputElement>('[data-testid="app-search"]')?.focus()
    return
  }
  if (!(event.ctrlKey || event.metaKey) || event.altKey) return
  const destination: Record<string, WorkspacePage> = {
    '1': 'home',
    '2': 'logs',
    '3': 'apps',
    '4': 'docs',
    ',': 'settings'
  }
  if (destination[event.key]) {
    event.preventDefault()
    page.value = destination[event.key]
  }
}

onMounted(() => {
  unsubscribeActivation = api.onInstanceActivated(() => {
    page.value = 'home'
  })
  unsubscribeNavigation = api.onNavigate((destination) => {
    page.value = destination
  })
  unsubscribeExplorer = api.onExplorerChanged((visible) => {
    explorerVisible.value = visible
  })
  window.addEventListener('keydown', shortcut)
})
onUnmounted(() => {
  unsubscribeActivation?.()
  unsubscribeNavigation?.()
  unsubscribeExplorer?.()
  window.clearTimeout(toastTimer)
  window.removeEventListener('keydown', shortcut)
})
</script>

<template>
  <div class="workspace">
    <aside class="sidebar">
      <div class="brand" title="VNet Studio" aria-label="VNet Studio">
        <div class="brand-symbol"><Icon name="activity" :size="22" /></div>
      </div>
      <nav class="primary-nav" aria-label="Main navigation">
        <button
          v-for="item in navigation"
          :key="item.id"
          :class="{ active: page === item.id }"
          :aria-current="page === item.id ? 'page' : undefined"
          :aria-expanded="item.id === 'home' && page === 'home' ? explorerVisible : undefined"
          :aria-controls="item.id === 'home' ? 'home-instance-list' : undefined"
          :aria-label="item.label"
          :title="
            item.id === 'home' && page === 'home'
              ? explorerVisible
                ? 'Hide instance list'
                : 'Show instance list'
              : item.label + ' (Ctrl / ⌘ ' + item.shortcut + ')'
          "
          :data-testid="'nav-' + item.id"
          @click="navigate(item.id)"
        >
          <Icon :name="item.icon" :size="20" />
        </button>
      </nav>
      <div class="sidebar-footer">
        <button
          class="settings-nav"
          :class="{ active: page === 'settings' }"
          :aria-current="page === 'settings' ? 'page' : undefined"
          aria-label="Settings"
          title="Settings (Ctrl / ⌘ ,)"
          data-testid="nav-settings"
          @click="page = 'settings'"
        >
          <Icon name="settings" :size="20" />
        </button>
      </div>
    </aside>

    <main class="workspace-main">
      <header class="captionbar">
        <MenuBar :explorer-visible="explorerVisible" @error="notify" />
        <span class="workspace-title">VNet Studio</span>
        <WindowControls />
      </header>
      <div v-if="toast && page === 'home'" class="workbench-notice" role="status">
        <span>{{ toast }}</span>
        <button aria-label="Dismiss notification" title="Dismiss notification" @click="toast = ''">
          <Icon name="close" :size="14" />
        </button>
      </div>
      <div :class="['page-scroll', { 'editor-scroll': ['home', 'logs', 'docs'].includes(page) }]">
        <Workbench
          v-if="page === 'home'"
          v-model:collapsed-categories="collapsedCategories"
          :state="state"
          :explorer-visible="explorerVisible"
          @browse="page = 'apps'"
          @error="notify"
        />

        <section v-else-if="page === 'apps'" class="page apps-page" data-testid="apps-page">
          <div class="library-toolbar">
            <div class="library-tab">
              All applications<span class="count-pill">{{ state.applications.length }}</span>
            </div>
            <label class="search-field"
              ><Icon name="search" :size="17" /><input
                v-model="query"
                aria-label="Search applications"
                placeholder="Search applications..."
                data-testid="app-search"
              /><kbd>/</kbd></label
            >
          </div>
          <div v-if="!filteredApps.length" class="search-empty">
            <Icon name="search" :size="30" />
            <h3>No applications found</h3>
            <p>Try another name or keyword.</p>
            <button class="button secondary" @click="query = ''">Clear search</button>
          </div>
          <div v-else class="library-grid">
            <article
              v-for="application in filteredApps"
              :key="application.id"
              class="application-card"
              :aria-labelledby="'app-title-' + application.id"
            >
              <div class="app-card-main">
                <div class="application-icon"><Icon name="activity" :size="28" /></div>
                <div class="app-card-information">
                  <h3 :id="'app-title-' + application.id">{{ application.name }}</h3>
                  <p class="app-description" :title="application.description">
                    {{ application.description }}
                  </p>
                  <p class="app-version">
                    Version <strong>{{ application.version }}</strong>
                  </p>
                </div>
              </div>
              <div class="app-launch-row">
                <label class="toggle-label">
                  <input
                    type="checkbox"
                    class="toggle"
                    :checked="application.enabled"
                    :disabled="!!pending"
                    data-testid="app-enabled"
                    @change="setEnabled(application, $event)"
                  />
                  <span>{{ application.enabled ? 'Enabled' : 'Disabled' }}</span>
                </label>
                <button
                  class="button primary"
                  :disabled="!application.enabled || !!pending"
                  :aria-label="'Launch ' + application.name"
                  data-testid="launch-demo"
                  @click="launch(application)"
                >
                  <Icon name="play" :size="15" />{{
                    pending === 'launch:' + application.id
                      ? 'Launching...'
                      : openCount(application.id)
                        ? 'Launch another'
                        : 'Launch app'
                  }}
                </button>
              </div>
            </article>
          </div>
        </section>

        <DocsReader
          v-else-if="page === 'docs'"
          v-model:chapter="docsChapter"
          :applications="state.applications"
        />

        <LogViewer v-else-if="page === 'logs'" @error="notify" />

        <section v-else class="page settings-page" data-testid="settings-page">
          <div class="page-heading">
            <div>
              <div class="eyebrow">YOUR PREFERENCES</div>
              <h1>A workspace that feels right<span class="heading-dot">.</span></h1>
              <p>Small adjustments, a more comfortable workspace.</p>
            </div>
          </div>
          <section class="settings-card">
            <div class="settings-section-heading">
              <span class="summary-icon blue"><Icon name="sun" /></span>
              <div>
                <h2>Appearance</h2>
                <p>Choose the look of your workspace and application windows.</p>
              </div>
            </div>
            <div class="theme-options">
              <button
                v-for="theme in themes"
                :key="theme.value"
                :class="['theme-option', { selected: state.settings.theme === theme.value }]"
                :aria-pressed="state.settings.theme === theme.value"
                :data-testid="'theme-' + theme.value"
                :disabled="!!pending"
                @click="chooseTheme(theme.value)"
              >
                <div :class="['theme-preview', theme.value]">
                  <span></span>
                  <div><i></i><i></i><i></i></div>
                </div>
                <span class="theme-name"
                  ><Icon :name="theme.icon" :size="17" />{{ theme.label
                  }}<Icon
                    v-if="state.settings.theme === theme.value"
                    class="theme-check"
                    name="check"
                    :size="16"
                /></span>
              </button>
            </div>
          </section>
          <section class="settings-card">
            <div class="settings-section-heading">
              <span class="summary-icon purple"><Icon name="window" /></span>
              <div>
                <h2>Application instances</h2>
                <p>Choose what happens when you close the workspace.</p>
              </div>
            </div>
            <label class="setting-row"
              ><div>
                <strong>Close apps with the workspace</strong>
                <p>
                  Close all application instances when you exit VNet Studio. When disabled, docked
                  pages move to independent windows.
                </p>
              </div>
              <input
                type="checkbox"
                class="toggle"
                :checked="state.settings.closeAppsOnExit"
                :disabled="!!pending"
                data-testid="close-apps-on-exit"
                @change="changeExitPreference"
            /></label>
          </section>
          <p class="settings-note">
            <Icon name="check" :size="15" />Your preferences are saved automatically.
          </p>
        </section>
        <footer v-if="page === 'apps' || page === 'settings'" class="workspace-footer">
          <span>VNet Studio</span><span>Automotive application workspace</span>
        </footer>
      </div>
    </main>
    <Transition name="toast"
      ><div v-if="toast && page !== 'home'" class="toast-message" role="status">
        <Icon name="check" :size="17" />{{ toast }}
      </div></Transition
    >
  </div>
</template>
