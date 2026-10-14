import type { LogEntry, LogLevel, LogSnapshot } from '../shared/workspace'

export class WorkspaceLogs {
  private entries: LogEntry[] = []
  private revision = 0
  private sequence = 0
  readonly limit = 500

  append(level: LogLevel, source: string, message: string): void {
    this.entries.push({
      id: ++this.sequence,
      timestamp: new Date().toISOString(),
      level,
      source: source.slice(0, 128),
      message: message.slice(0, 4096)
    })
    if (this.entries.length > this.limit) this.entries.shift()
    this.revision += 1
  }

  clear(): void {
    this.entries = []
    this.revision += 1
  }

  snapshot(): LogSnapshot {
    return {
      revision: this.revision,
      limit: this.limit,
      entries: this.entries.map((entry) => ({ ...entry }))
    }
  }
}
