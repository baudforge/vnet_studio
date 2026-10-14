import {
  app,
  BaseWindow,
  BrowserWindow,
  WebContentsView,
  Menu,
  ipcMain,
  nativeTheme,
  shell,
  type IpcMainInvokeEvent,
  type WebContents,
  type MenuItemConstructorOptions
} from 'electron'
import { join } from 'node:path'
import { randomUUID } from 'node:crypto'
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import { electronApp, is, optimizer } from '@electron-toolkit/utils'
import icon from '../../resources/icon.png?asset'
import { WorkspaceLogs } from './logs'
import type {
  ApplicationDefinition,
  ApplicationInstance,
  Bootstrap,
  DockBounds,
  LogLevel,
  MenuRequest,
  WorkspaceMenu,
  WorkspacePage,
  WorkspaceSettings,
  WorkspaceState
} from '../shared/workspace'

const catalog: Omit<ApplicationDefinition, 'enabled'>[] = [
  {
    id: 'bus-trace-demo',
    name: 'Bus Trace Demo',
    description: 'Explore a live CAN trace with simulated traffic. No hardware setup required.',
    category: 'Demo',
    version: '0.1.0',
    tags: ['CAN', 'Simulation', 'Trace']
  }
]
interface InstanceEntry {
  window: BaseWindow | null
  view: WebContentsView
  instance: ApplicationInstance
}
const instances = new Map<string, InstanceEntry>()
const logs = new WorkspaceLogs()
let workspaceWindow: BrowserWindow | null = null
let logSubscriber: WebContents | null = null
let logUpdateTimer: ReturnType<typeof setTimeout> | undefined
let activeInstanceId: string | null = null
let dockBounds: DockBounds | null = null
let quitting = false
let currentMenu: Menu | null = null
let revision = 0
let launches = 0
let preferencesPath = ''
let settings: WorkspaceSettings = { theme: 'light', closeAppsOnExit: true }
let disabledApplications: string[] = []

app.setName('VNet Studio')

function publishLogs(): void {
  if (!logSubscriber || logUpdateTimer) return
  logUpdateTimer = setTimeout(() => {
    logUpdateTimer = undefined
    if (logSubscriber && !logSubscriber.isDestroyed()) {
      logSubscriber.send('logs:changed', logs.snapshot())
    }
  }, 100)
  logUpdateTimer.unref()
}

function recordLog(level: LogLevel, source: string, message: string): void {
  logs.append(level, source, message)
  publishLogs()
}

function instanceLabel(instance: ApplicationInstance): string {
  return `${instance.name} ${String(instance.ordinal).padStart(2, '0')}`
}

function watchRendererLogs(contents: WebContents, source: string): void {
  contents.on('console-message', (event) => {
    if (event.level === 'warning' || event.level === 'error') {
      recordLog(event.level, source, event.message)
    }
  })
  contents.on('render-process-gone', (_event, details) => {
    if (details.reason !== 'clean-exit') {
      recordLog('error', source, `Renderer stopped: ${details.reason} (${details.exitCode}).`)
    }
  })
}

function snapshot(): WorkspaceState {
  return {
    revision,
    applications: catalog.map((application) => ({
      ...application,
      enabled: !disabledApplications.includes(application.id)
    })),
    instances: [...instances.values()]
      .map(({ instance }) => ({ ...instance }))
      .sort((left, right) => left.ordinal - right.ordinal),
    settings: { ...settings },
    launches,
    activeInstanceId
  }
}

function publish(): void {
  layoutDockedViews()
  revision += 1
  const state = snapshot()
  const contents = [
    workspaceWindow?.webContents,
    ...[...instances.values()].map((entry) => entry.view.webContents)
  ]
  for (const content of contents) {
    if (content && !content.isDestroyed()) content.send('workspace:state', state)
  }
}

function layoutDockedViews(): void {
  if (!workspaceWindow || workspaceWindow.isDestroyed()) return
  const [width, height] = workspaceWindow.getContentSize()
  for (const { view, instance } of instances.values()) {
    if (instance.placement !== 'docked') continue
    const visible = !!dockBounds && instance.id === activeInstanceId
    view.setVisible(visible)
    if (dockBounds) {
      const x = Math.min(width, Math.max(0, Math.round(dockBounds.x)))
      const y = Math.min(height, Math.max(0, Math.round(dockBounds.y)))
      view.setBounds({
        x,
        y,
        width: Math.max(0, Math.min(width - x, Math.round(dockBounds.width))),
        height: Math.max(0, Math.min(height - y, Math.round(dockBounds.height)))
      })
    }
  }
}

function selectNextInstance(): void {
  activeInstanceId =
    [...instances.values()].find((entry) => entry.instance.placement === 'docked')?.instance.id ??
    null
}

function closeInstance(instanceId: string): void {
  const entry = instances.get(instanceId)
  if (!entry) return
  instances.delete(instanceId)
  workspaceWindow?.contentView.removeChildView(entry.view)
  if (entry.window && !entry.window.isDestroyed()) {
    entry.window.contentView.removeChildView(entry.view)
    entry.window.destroy()
  }
  const contents = entry.view.webContents
  if (contents && !contents.isDestroyed()) contents.close()
  if (activeInstanceId === instanceId) selectNextInstance()
  publish()
  recordLog('info', instanceLabel(entry.instance), 'Instance closed.')
}

function activateInstance(instanceId: string): void {
  activeInstanceId = instanceId
  publish()
  workspaceWindow?.webContents.send('workspace:activated', instanceId)
}

function detachInstance(instanceId: string): void {
  const entry = instances.get(instanceId)
  if (!entry) throw new Error('This application has already closed.')
  if (entry.window) {
    entry.window.show()
    entry.window.focus()
    return
  }
  const window = new BaseWindow({
    title: `${entry.instance.name} · Instance ${entry.instance.ordinal} · VNet Studio`,
    width: 1100,
    height: 820,
    minWidth: 800,
    minHeight: 620,
    frame: false,
    show: false,
    autoHideMenuBar: true,
    backgroundColor: nativeTheme.shouldUseDarkColors ? '#101722' : '#f5f7fa'
  })
  workspaceWindow?.contentView.removeChildView(entry.view)
  entry.window = window
  entry.instance.placement = 'detached'
  window.contentView.addChildView(entry.view)
  const resize = (): void => {
    const [width, height] = window.getContentSize()
    entry.view.setBounds({ x: 0, y: 0, width, height })
  }
  window.on('resize', resize)
  window.on('closed', () => {
    // Re-docking transfers the view before destroying its former host.
    if (entry.window === window) closeInstance(instanceId)
  })
  resize()
  entry.view.setVisible(true)
  if (activeInstanceId === instanceId) selectNextInstance()
  publish()
  window.show()
  window.focus()
  recordLog('info', instanceLabel(entry.instance), 'Moved to an external window.')
}

function dockInstance(instanceId: string): void {
  const entry = instances.get(instanceId)
  if (!entry) throw new Error('This application has already closed.')
  showWorkspace()
  const formerWindow = entry.window
  if (formerWindow) formerWindow.contentView.removeChildView(entry.view)
  entry.window = null
  entry.instance.placement = 'docked'
  workspaceWindow!.contentView.addChildView(entry.view)
  entry.view.setVisible(false)
  activateInstance(instanceId)
  formerWindow?.destroy()
  if (formerWindow) recordLog('info', instanceLabel(entry.instance), 'Docked to Home.')
}

function persist(nextSettings = settings, nextDisabledApplications = disabledApplications): void {
  writeFileSync(
    preferencesPath,
    JSON.stringify(
      { settings: nextSettings, disabledApplications: nextDisabledApplications },
      null,
      2
    )
  )
}

function loadPreferences(): void {
  mkdirSync(app.getPath('userData'), { recursive: true })
  preferencesPath = join(app.getPath('userData'), 'workspace-preferences.json')
  if (!existsSync(preferencesPath)) return
  try {
    const saved = JSON.parse(readFileSync(preferencesPath, 'utf8'))
    if (['light', 'dark', 'system'].includes(saved.settings?.theme)) {
      settings.theme = saved.settings.theme
    }
    if (typeof saved.settings?.closeAppsOnExit === 'boolean') {
      settings.closeAppsOnExit = saved.settings.closeAppsOnExit
    }
    if (Array.isArray(saved.disabledApplications)) {
      disabledApplications = saved.disabledApplications.filter((id: unknown) =>
        catalog.some((application) => application.id === id)
      )
    }
  } catch {
    console.warn('Could not read workspace preferences; using defaults.')
    recordLog('warning', 'Workspace', 'Could not read workspace preferences; using defaults.')
  }
}

function ownedWindow(event: IpcMainInvokeEvent): BaseWindow {
  if (event.sender === workspaceWindow?.webContents) return workspaceWindow
  const entry = [...instances.values()].find((item) => item.view.webContents === event.sender)
  const window = entry?.window ?? (entry ? workspaceWindow : null)
  if (!window) throw new Error('Unknown application window.')
  return window
}

function requireWorkspace(event: IpcMainInvokeEvent): void {
  if (event.sender !== workspaceWindow?.webContents) {
    throw new Error('Application management is available in the workspace.')
  }
}

function requireInstanceAccess(event: IpcMainInvokeEvent, instanceId: string): void {
  if (event.sender === workspaceWindow?.webContents) return
  if (instances.get(instanceId)?.view.webContents !== event.sender) {
    throw new Error('This application instance belongs to another page.')
  }
}

function showMenu(request: MenuRequest): Promise<void> {
  const window = workspaceWindow!
  currentMenu?.closePopup(window)
  const active = activeInstanceId ? instances.get(activeInstanceId) : undefined
  const editor = dockBounds && active ? active.view.webContents : window.webContents
  const navigate = (page: WorkspacePage): void =>
    window.webContents.send('workspace:navigate', page)
  const edit = (action: 'undo' | 'redo' | 'cut' | 'copy' | 'paste' | 'selectAll'): void => {
    if (!editor.isDestroyed()) {
      editor.focus()
      editor[action]()
    }
  }
  const templates: Record<MenuRequest['menu'], MenuItemConstructorOptions[]> = {
    file: [
      { id: 'new-application', label: 'Open Application...', click: () => navigate('apps') },
      {
        id: 'close-application',
        label: 'Close Active Instance',
        enabled: !!active,
        click: () => {
          if (active) closeInstance(active.instance.id)
        }
      },
      { type: 'separator' },
      {
        id: 'preferences',
        label: 'Settings',
        accelerator: 'CmdOrCtrl+,',
        click: () => navigate('settings')
      },
      { type: 'separator' },
      { id: 'close-workspace', label: 'Close Workspace', click: () => window.close() }
    ],
    edit: [
      { id: 'undo', label: 'Undo', accelerator: 'CmdOrCtrl+Z', click: () => edit('undo') },
      { id: 'redo', label: 'Redo', accelerator: 'CmdOrCtrl+Shift+Z', click: () => edit('redo') },
      { type: 'separator' },
      { id: 'cut', label: 'Cut', accelerator: 'CmdOrCtrl+X', click: () => edit('cut') },
      { id: 'copy', label: 'Copy', accelerator: 'CmdOrCtrl+C', click: () => edit('copy') },
      { id: 'paste', label: 'Paste', accelerator: 'CmdOrCtrl+V', click: () => edit('paste') },
      { type: 'separator' },
      {
        id: 'select-all',
        label: 'Select All',
        accelerator: 'CmdOrCtrl+A',
        click: () => edit('selectAll')
      }
    ],
    view: [
      { id: 'home', label: 'Home', accelerator: 'CmdOrCtrl+1', click: () => navigate('home') },
      {
        id: 'logs',
        label: 'Logs',
        accelerator: 'CmdOrCtrl+2',
        click: () => navigate('logs')
      },
      {
        id: 'applications',
        label: 'Apps',
        accelerator: 'CmdOrCtrl+3',
        click: () => navigate('apps')
      },
      {
        id: 'docs',
        label: 'Docs',
        accelerator: 'CmdOrCtrl+4',
        click: () => navigate('docs')
      },
      { type: 'separator' },
      {
        id: 'instance-list',
        label: 'Home Instance List',
        type: 'checkbox',
        checked: request.explorerVisible,
        click: () => {
          navigate('home')
          window.webContents.send('workspace:explorer', !request.explorerVisible)
        }
      },
      {
        id: 'detach-application',
        label: 'Move Active Instance to Window',
        enabled: !!active,
        click: () => {
          if (active) detachInstance(active.instance.id)
        }
      }
    ],
    help: [
      {
        id: 'documentation',
        label: 'Docs',
        accelerator: 'CmdOrCtrl+4',
        click: () => navigate('docs')
      },
      {
        id: 'repository',
        label: 'Project Repository',
        click: () => {
          void shell.openExternal('https://github.com/baudforge/vnet_studio')
        }
      }
    ]
  }
  const menu = Menu.buildFromTemplate(templates[request.menu])
  currentMenu = menu
  const [width, height] = window.getContentSize()
  return new Promise((resolve) => {
    menu.popup({
      window,
      x: Math.min(width, Math.round(request.x)),
      y: Math.min(height, Math.round(request.y)),
      callback: () => {
        if (currentMenu === menu) currentMenu = null
        resolve()
      }
    })
  })
}

function makeWindow(title: string): BrowserWindow {
  const window = new BrowserWindow({
    title,
    width: 1280,
    height: 840,
    minWidth: 900,
    minHeight: 620,
    show: false,
    frame: false,
    autoHideMenuBar: true,
    backgroundColor: nativeTheme.shouldUseDarkColors ? '#101722' : '#f5f7fa',
    ...(process.platform === 'linux' ? { icon } : {}),
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false
    }
  })
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }))
  watchRendererLogs(window.webContents, 'Workspace')
  return window
}

async function loadContents(contents: WebContents): Promise<void> {
  if (is.dev && process.env.ELECTRON_RENDERER_URL) {
    await contents.loadURL(process.env.ELECTRON_RENDERER_URL)
  } else {
    await contents.loadFile(join(__dirname, '../renderer/index.html'))
  }
}

function showWorkspace(): void {
  if (workspaceWindow && !workspaceWindow.isDestroyed()) {
    if (workspaceWindow.isMinimized()) workspaceWindow.restore()
    workspaceWindow.show()
    workspaceWindow.focus()
    return
  }
  const window = makeWindow('VNet Studio')
  workspaceWindow = window
  dockBounds = null
  window.on('resize', layoutDockedViews)
  window.once('ready-to-show', () => {
    window.show()
    recordLog('info', 'Workspace', 'Workspace opened.')
  })
  window.on('close', () => {
    currentMenu?.closePopup(window)
    if (settings.closeAppsOnExit || quitting) {
      for (const id of [...instances.keys()]) closeInstance(id)
    } else {
      for (const { instance } of [...instances.values()]) {
        if (instance.placement === 'docked') detachInstance(instance.id)
      }
    }
  })
  window.on('closed', () => {
    logSubscriber = null
    workspaceWindow = null
  })
  void loadContents(window.webContents).catch((error) => {
    console.error('Could not open workspace:', error)
    recordLog('error', 'Workspace', `Could not open workspace: ${String(error)}`)
    window.destroy()
  })
}

async function launchApplication(applicationId: string): Promise<ApplicationInstance> {
  const application = catalog.find((entry) => entry.id === applicationId)
  if (!application || disabledApplications.includes(applicationId)) {
    throw new Error('This application is unavailable.')
  }
  const instance: ApplicationInstance = {
    id: randomUUID(),
    applicationId,
    name: application.name,
    ordinal: ++launches,
    startedAt: new Date().toISOString(),
    status: 'starting',
    placement: 'docked'
  }
  const view = new WebContentsView({
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
      backgroundThrottling: false
    }
  })
  view.webContents.setWindowOpenHandler(() => ({ action: 'deny' }))
  watchRendererLogs(view.webContents, instanceLabel(instance))
  view.webContents.on('before-input-event', (event, input) => {
    if (
      input.type === 'keyDown' &&
      input.alt &&
      !input.control &&
      !input.meta &&
      instance.placement === 'docked'
    ) {
      const menus: Record<string, WorkspaceMenu> = { f: 'file', e: 'edit', v: 'view', h: 'help' }
      const menu = menus[input.key.toLowerCase()]
      if (menu) {
        event.preventDefault()
        showWorkspace()
        workspaceWindow?.webContents.send('workspace:menu-requested', menu)
      }
      return
    }
    if (input.type !== 'keyDown' || !(input.control || input.meta) || input.alt) return
    const destinations: Record<string, WorkspacePage> = {
      '1': 'home',
      '2': 'logs',
      '3': 'apps',
      '4': 'docs',
      ',': 'settings'
    }
    const destination = destinations[input.key]
    if (destination) {
      event.preventDefault()
      showWorkspace()
      workspaceWindow?.webContents.send('workspace:navigate', destination)
    }
  })
  view.setVisible(false)
  instances.set(instance.id, { window: null, view, instance })
  workspaceWindow!.contentView.addChildView(view)
  view.webContents.on('destroyed', () => closeInstance(instance.id))
  activateInstance(instance.id)
  try {
    await loadContents(view.webContents)
    if (instances.has(instance.id)) {
      instance.status = 'running'
      publish()
      recordLog('info', instanceLabel(instance), 'Instance started.')
    }
    return { ...instance }
  } catch (error) {
    closeInstance(instance.id)
    throw error
  }
}

function registerApi(): void {
  ipcMain.on('logs:watch', (event, subscribe: boolean) => {
    if (event.sender !== workspaceWindow?.webContents || typeof subscribe !== 'boolean') return
    logSubscriber = subscribe ? event.sender : null
  })
  const handle = (channel: string, listener: Parameters<typeof ipcMain.handle>[1]): void => {
    ipcMain.handle(channel, async (event, ...args: unknown[]) => {
      try {
        return await listener(event, ...args)
      } catch (error) {
        const instance = [...instances.values()].find(
          (entry) => entry.view.webContents === event.sender
        )?.instance
        recordLog(
          'error',
          instance ? instanceLabel(instance) : 'Workspace',
          `${channel}: ${error instanceof Error ? error.message : String(error)}`
        )
        throw error
      }
    })
  }
  handle('logs:get', (event) => {
    requireWorkspace(event)
    return logs.snapshot()
  })
  handle('logs:clear', (event) => {
    requireWorkspace(event)
    logs.clear()
    publishLogs()
  })
  handle('documentation:open-link', (event, address: string) => {
    requireWorkspace(event)
    if (typeof address !== 'string') throw new Error('Invalid documentation link.')
    const url = new URL(address)
    if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password) {
      throw new Error('Only web links can be opened from documentation.')
    }
    return shell.openExternal(url.href)
  })
  handle('workspace:menu', (event, request: MenuRequest) => {
    requireWorkspace(event)
    if (
      !request ||
      !['file', 'edit', 'view', 'help'].includes(request.menu) ||
      typeof request.explorerVisible !== 'boolean' ||
      ![request.x, request.y].every(
        (value) => typeof value === 'number' && Number.isFinite(value) && value >= 0
      )
    ) {
      throw new Error('Invalid workspace menu.')
    }
    return showMenu(request)
  })
  handle('workspace:bootstrap', (event): Bootstrap => {
    ownedWindow(event)
    const entry = [...instances.values()].find((item) => item.view.webContents === event.sender)
    return {
      context: entry
        ? { kind: 'application', instance: { ...entry.instance } }
        : { kind: 'workspace' },
      state: snapshot()
    }
  })
  handle('workspace:launch', (event, applicationId: string) => {
    requireWorkspace(event)
    return launchApplication(applicationId)
  })
  handle('workspace:focus', (event, instanceId: string) => {
    requireWorkspace(event)
    const entry = instances.get(instanceId)
    if (!entry) throw new Error('This application has already closed.')
    if (entry.window) {
      if (entry.window.isMinimized()) entry.window.restore()
      entry.window.show()
      entry.window.focus()
    } else {
      activateInstance(instanceId)
      entry.view.webContents.focus()
    }
  })
  handle('workspace:close', (event, instanceId: string) => {
    requireWorkspace(event)
    closeInstance(instanceId)
  })
  handle('workspace:detach', (event, instanceId: string) => {
    requireInstanceAccess(event, instanceId)
    detachInstance(instanceId)
  })
  handle('workspace:dock', (event, instanceId: string) => {
    requireInstanceAccess(event, instanceId)
    dockInstance(instanceId)
  })
  handle('workspace:bounds', (event, bounds: DockBounds | null) => {
    requireWorkspace(event)
    if (
      bounds !== null &&
      (!bounds ||
        !['x', 'y', 'width', 'height'].every((key) => {
          const value = bounds[key as keyof DockBounds]
          return typeof value === 'number' && Number.isFinite(value) && value >= 0
        }))
    )
      throw new Error('Invalid editor bounds.')
    dockBounds = bounds
    layoutDockedViews()
  })
  handle('workspace:enabled', (event, applicationId: string, enabled: boolean) => {
    requireWorkspace(event)
    if (typeof enabled !== 'boolean' || !catalog.some((entry) => entry.id === applicationId)) {
      throw new Error('Invalid application preference.')
    }
    const nextDisabledApplications = disabledApplications.filter((id) => id !== applicationId)
    if (!enabled) nextDisabledApplications.push(applicationId)
    persist(settings, nextDisabledApplications)
    disabledApplications = nextDisabledApplications
    publish()
    recordLog('info', 'Workspace', `${applicationId} ${enabled ? 'enabled' : 'disabled'}.`)
  })
  handle('workspace:settings', (event, changes: Partial<WorkspaceSettings>) => {
    requireWorkspace(event)
    if (!changes || typeof changes !== 'object') throw new Error('Invalid workspace settings.')
    if (changes.theme !== undefined) {
      if (!['light', 'dark', 'system'].includes(changes.theme)) throw new Error('Invalid theme.')
    }
    if (changes.closeAppsOnExit !== undefined) {
      if (typeof changes.closeAppsOnExit !== 'boolean') throw new Error('Invalid exit preference.')
    }
    const nextSettings = {
      theme: changes.theme ?? settings.theme,
      closeAppsOnExit: changes.closeAppsOnExit ?? settings.closeAppsOnExit
    }
    persist(nextSettings)
    settings = nextSettings
    nativeTheme.themeSource = settings.theme
    publish()
    recordLog('info', 'Workspace', 'Preferences saved.')
  })
  handle('workspace:show', (event) => {
    ownedWindow(event)
    showWorkspace()
  })
  handle('window:action', (event, action: string) => {
    const window = ownedWindow(event)
    if (action === 'minimize') window.minimize()
    else if (action === 'maximize') {
      if (window.isMaximized()) window.unmaximize()
      else window.maximize()
    } else if (action === 'close') window.close()
    else throw new Error('Invalid window action.')
  })
  handle('workspace:repository', (event) => {
    ownedWindow(event)
    return shell.openExternal('https://github.com/baudforge/vnet_studio')
  })
}

app.whenReady().then(() => {
  electronApp.setAppUserModelId('com.baudforge.vnetstudio')
  Menu.setApplicationMenu(null)
  recordLog('info', 'Workspace', 'VNet Studio started.')
  loadPreferences()
  nativeTheme.themeSource = settings.theme
  app.on('browser-window-created', (_, window) => optimizer.watchWindowShortcuts(window))
  registerApi()
  showWorkspace()
  app.on('activate', showWorkspace)
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})

app.on('before-quit', () => {
  quitting = true
  for (const id of [...instances.keys()]) closeInstance(id)
})
