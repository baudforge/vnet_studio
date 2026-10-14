import assert from 'node:assert/strict'
import fs from 'node:fs'
import test from 'node:test'
import ts from 'typescript'

const source = fs.readFileSync(new URL('../src/main/logs.ts', import.meta.url), 'utf8')
const compiled = ts.transpileModule(source, {
  compilerOptions: { module: ts.ModuleKind.ES2022, target: ts.ScriptTarget.ES2022 }
})
const { WorkspaceLogs } = await import(
  'data:text/javascript;base64,' + Buffer.from(compiled.outputText).toString('base64')
)

test('retains the latest 500 messages in chronological order', () => {
  const logs = new WorkspaceLogs()
  for (let index = 0; index < 700; index++) logs.append('info', 'Workspace', String(index))
  const snapshot = logs.snapshot()
  assert.equal(snapshot.limit, 500)
  assert.equal(snapshot.entries.length, 500)
  assert.equal(snapshot.entries[0].message, '200')
  assert.equal(snapshot.entries.at(-1).message, '699')
  assert(snapshot.entries.every((entry, index) => entry.id === index + 201))
})

test('clearing advances the revision and preserves unique entry IDs', () => {
  const logs = new WorkspaceLogs()
  logs.append('warning', 'Workspace', 'Before clear')
  const before = logs.snapshot()
  logs.clear()
  const cleared = logs.snapshot()
  assert(cleared.revision > before.revision)
  assert.deepEqual(cleared.entries, [])
  logs.append('error', 'App 01', 'After clear')
  const after = logs.snapshot()
  assert(after.revision > cleared.revision)
  assert(after.entries[0].id > before.entries[0].id)
  assert.equal(after.entries[0].level, 'error')
})

test('snapshots do not expose mutable log storage', () => {
  const logs = new WorkspaceLogs()
  logs.append('info', 'Workspace', 'Original')
  const snapshot = logs.snapshot()
  snapshot.entries[0].message = 'Changed'
  snapshot.entries.push(snapshot.entries[0])
  assert.equal(logs.snapshot().entries.length, 1)
  assert.equal(logs.snapshot().entries[0].message, 'Original')
})

test('bounds individual payload sizes as well as entry count', () => {
  const logs = new WorkspaceLogs()
  logs.append('error', 'S'.repeat(1000), 'M'.repeat(100000))
  const entry = logs.snapshot().entries[0]
  assert.equal(entry.source.length, 128)
  assert.equal(entry.message.length, 4096)
  assert(!Number.isNaN(Date.parse(entry.timestamp)))
})
