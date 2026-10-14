import { contextBridge, ipcRenderer } from 'electron'
import type {
  LogSnapshot,
  WorkspaceApi,
  WorkspaceMenu,
  WorkspacePage,
  WorkspaceState
} from '../shared/workspace'

const api: WorkspaceApi = {
  getBootstrap: () => ipcRenderer.invoke('workspace:bootstrap'),
  launchApplication: (applicationId) => ipcRenderer.invoke('workspace:launch', applicationId),
  focusInstance: (instanceId) => ipcRenderer.invoke('workspace:focus', instanceId),
  closeInstance: (instanceId) => ipcRenderer.invoke('workspace:close', instanceId),
  detachInstance: (instanceId) => ipcRenderer.invoke('workspace:detach', instanceId),
  dockInstance: (instanceId) => ipcRenderer.invoke('workspace:dock', instanceId),
  setDockBounds: (bounds) => ipcRenderer.invoke('workspace:bounds', bounds),
  setApplicationEnabled: (applicationId, enabled) =>
    ipcRenderer.invoke('workspace:enabled', applicationId, enabled),
  updateSettings: (settings) => ipcRenderer.invoke('workspace:settings', settings),
  showWorkspace: () => ipcRenderer.invoke('workspace:show'),
  windowAction: (action) => ipcRenderer.invoke('window:action', action),
  openRepository: () => ipcRenderer.invoke('workspace:repository'),
  openDocumentationLink: (url) => ipcRenderer.invoke('documentation:open-link', url),
  openMenu: (request) => ipcRenderer.invoke('workspace:menu', request),
  getLogs: () => ipcRenderer.invoke('logs:get'),
  clearLogs: () => ipcRenderer.invoke('logs:clear'),
  onLogsChanged: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, logs: LogSnapshot): void => callback(logs)
    ipcRenderer.on('logs:changed', listener)
    ipcRenderer.send('logs:watch', true)
    return () => {
      ipcRenderer.removeListener('logs:changed', listener)
      ipcRenderer.send('logs:watch', false)
    }
  },
  onMenuRequested: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, menu: WorkspaceMenu): void =>
      callback(menu)
    ipcRenderer.on('workspace:menu-requested', listener)
    return () => ipcRenderer.removeListener('workspace:menu-requested', listener)
  },
  onExplorerChanged: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, visible: boolean): void =>
      callback(visible)
    ipcRenderer.on('workspace:explorer', listener)
    return () => ipcRenderer.removeListener('workspace:explorer', listener)
  },
  onNavigate: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, page: WorkspacePage): void =>
      callback(page)
    ipcRenderer.on('workspace:navigate', listener)
    return () => ipcRenderer.removeListener('workspace:navigate', listener)
  },
  onInstanceActivated: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, instanceId: string): void =>
      callback(instanceId)
    ipcRenderer.on('workspace:activated', listener)
    return () => ipcRenderer.removeListener('workspace:activated', listener)
  },
  onStateChanged: (callback) => {
    const listener = (_event: Electron.IpcRendererEvent, state: WorkspaceState): void =>
      callback(state)
    ipcRenderer.on('workspace:state', listener)
    return () => ipcRenderer.removeListener('workspace:state', listener)
  }
}

contextBridge.exposeInMainWorld('api', api)
