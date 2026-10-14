import type { WorkspaceApi } from '../shared/workspace'

declare global {
  interface Window {
    api: WorkspaceApi
  }
}
