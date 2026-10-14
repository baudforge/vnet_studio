export type Theme = 'light' | 'dark' | 'system'
export type InstanceStatus = 'starting' | 'running'
export type InstancePlacement = 'docked' | 'detached'
export type WorkspacePage = 'home' | 'logs' | 'apps' | 'docs' | 'settings'
export type WorkspaceMenu = 'file' | 'edit' | 'view' | 'help'

export type LogLevel = 'info' | 'warning' | 'error'

export interface LogEntry {
  id: number
  timestamp: string
  level: LogLevel
  source: string
  message: string
}

export interface LogSnapshot {
  revision: number
  limit: number
  entries: LogEntry[]
}

export interface MenuRequest {
  menu: WorkspaceMenu
  x: number
  y: number
  explorerVisible: boolean
}

export interface DockBounds {
  x: number
  y: number
  width: number
  height: number
}

export interface ApplicationDefinition {
  id: string
  name: string
  description: string
  category: string
  version: string
  tags: string[]
  enabled: boolean
}

export interface ApplicationInstance {
  id: string
  applicationId: string
  name: string
  ordinal: number
  startedAt: string
  status: InstanceStatus
  placement: InstancePlacement
}

export interface WorkspaceSettings {
  theme: Theme
  closeAppsOnExit: boolean
}

export interface WorkspaceState {
  revision: number
  applications: ApplicationDefinition[]
  instances: ApplicationInstance[]
  settings: WorkspaceSettings
  launches: number
  activeInstanceId: string | null
}

export type WindowContext =
  { kind: 'workspace' } | { kind: 'application'; instance: ApplicationInstance }

export interface Bootstrap {
  context: WindowContext
  state: WorkspaceState
}

export interface WorkspaceApi {
  getBootstrap: () => Promise<Bootstrap>
  launchApplication: (applicationId: string) => Promise<ApplicationInstance>
  focusInstance: (instanceId: string) => Promise<void>
  closeInstance: (instanceId: string) => Promise<void>
  detachInstance: (instanceId: string) => Promise<void>
  dockInstance: (instanceId: string) => Promise<void>
  setDockBounds: (bounds: DockBounds | null) => Promise<void>
  setApplicationEnabled: (applicationId: string, enabled: boolean) => Promise<void>
  updateSettings: (settings: Partial<WorkspaceSettings>) => Promise<void>
  showWorkspace: () => Promise<void>
  windowAction: (action: 'minimize' | 'maximize' | 'close') => Promise<void>
  openRepository: () => Promise<void>
  openDocumentationLink: (url: string) => Promise<void>
  openMenu: (request: MenuRequest) => Promise<void>
  getLogs: () => Promise<LogSnapshot>
  clearLogs: () => Promise<void>
  onLogsChanged: (callback: (logs: LogSnapshot) => void) => () => void
  onStateChanged: (callback: (state: WorkspaceState) => void) => () => void
  onInstanceActivated: (callback: (instanceId: string) => void) => () => void
  onNavigate: (callback: (page: WorkspacePage) => void) => () => void
  onExplorerChanged: (callback: (visible: boolean) => void) => () => void
  onMenuRequested: (callback: (menu: WorkspaceMenu) => void) => () => void
}
