import assert from 'node:assert/strict'
import fs from 'node:fs'
import test from 'node:test'
import ts from 'typescript'

const source = fs.readFileSync(
  new URL('../src/renderer/src/instance-groups.ts', import.meta.url),
  'utf8'
)
const compiled = ts.transpileModule(source, {
  compilerOptions: { module: ts.ModuleKind.ES2022, target: ts.ScriptTarget.ES2022 }
})
const { groupInstancesByCategory } = await import(
  'data:text/javascript;base64,' + Buffer.from(compiled.outputText).toString('base64')
)
const application = (id, category) => ({
  id,
  name: id,
  description: '',
  category,
  version: '0.1.0',
  tags: [],
  enabled: true
})
const instance = (id, applicationId, placement = 'docked') => ({
  id,
  applicationId,
  name: applicationId,
  ordinal: Number(id),
  startedAt: '2026-10-13T00:00:00.000Z',
  status: 'running',
  placement
})

test('groups multiple applications by category and retains all instances', () => {
  const state = {
    applications: [
      application('analyzer', 'CAN'),
      application('diagnosis', 'Diagnostics'),
      application('bootloader', 'Diagnostics'),
      application('demo', 'Demo')
    ],
    instances: [
      instance('1', 'analyzer'),
      instance('2', 'diagnosis'),
      instance('3', 'bootloader'),
      instance('4', 'demo')
    ]
  }
  const before = structuredClone(state)
  const groups = groupInstancesByCategory(state)
  assert.deepEqual(
    groups.map((group) => group.name),
    ['CAN', 'Demo', 'Diagnostics']
  )
  assert.deepEqual(
    groups.find((group) => group.name === 'Diagnostics').instances.map((entry) => entry.id),
    ['2', '3']
  )
  assert.equal(groups.flatMap((group) => group.instances).length, 4)
  assert.deepEqual(state, before, 'Grouping must not change workspace state')
})

test('docked and detached instances share their category without empty categories', () => {
  const groups = groupInstancesByCategory({
    applications: [application('trace', 'Demo'), application('unused', 'Diagnostics')],
    instances: [instance('1', 'trace'), instance('2', 'trace', 'detached')]
  })
  assert.equal(groups.length, 1)
  assert.deepEqual(
    groups[0].instances.map((entry) => entry.placement),
    ['docked', 'detached']
  )
})

test('category names are trimmed and unknown applications remain visible in Other', () => {
  const groups = groupInstancesByCategory({
    applications: [
      application('one', ' Demo '),
      application('two', 'Demo'),
      application('blank', ' ')
    ],
    instances: [
      instance('1', 'one'),
      instance('2', 'two'),
      instance('3', 'blank'),
      instance('4', 'unregistered')
    ]
  })
  assert.deepEqual(
    groups.map((group) => [group.name, group.instances.length]),
    [
      ['Demo', 2],
      ['Other', 2]
    ]
  )
})

test('an empty workspace has no category headers', () => {
  assert.deepEqual(
    groupInstancesByCategory({ applications: [application('demo', 'Demo')], instances: [] }),
    []
  )
})
