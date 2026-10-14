import { app, BaseWindow, BrowserWindow, WebContentsView, Menu, nativeImage } from 'electron'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const profile = path.join(root, 'node_modules/.cache/workspace-smoke-profile')
const screenshots = path.join(root, 'node_modules/.cache/workspace-screenshots')
fs.mkdirSync(screenshots, { recursive: true })
fs.mkdirSync(profile, { recursive: true })
fs.writeFileSync(
  path.join(profile, 'workspace-preferences.json'),
  JSON.stringify({ settings: { theme: 'light', closeAppsOnExit: true }, disabledApplications: [] })
)
app.setPath('userData', profile)
app.disableHardwareAcceleration()
app.commandLine.appendSwitch('disable-features', 'CalculateNativeWinOcclusion')
app.commandLine.appendSwitch('disable-backgrounding-occluded-windows')
app.on('web-contents-created', (_, contents) => contents.setBackgroundThrottling(false))
const focusedWindows = new Set()
const menuPopups = []
Menu.prototype.popup = function (options) {
  // Capture real Menu objects to exercise their commands without an interactive native popup.
  menuPopups.push({ menu: this, options })
}
Menu.prototype.closePopup = function () {
  const popup = menuPopups.findLast((popup) => popup.menu === this)
  popup?.options.callback?.()
}
for (const Window of [BaseWindow, BrowserWindow]) {
  Window.prototype.show = function () {
    // Native child surfaces need a shown host to paint. Avoid taking focus during the test.
    this.setIgnoreMouseEvents(true)
    this.setSkipTaskbar(true)
    this.showInactive()
  }
  Window.prototype.focus = function () {
    focusedWindows.add(this.id)
  }
}
const holdQuit = (event) => event.preventDefault()
app.on('will-quit', holdQuit)
const deadline = setTimeout(() => {
  console.error('Workspace smoke test timed out')
  app.exit(1)
}, 60000)
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms))
async function waitFor(check, label) {
  const started = Date.now()
  while (Date.now() - started < 12000) {
    if (await check()) return
    await delay(50)
  }
  throw new Error('Timed out: ' + label)
}
const evaluate = (target, script) => target.webContents.executeJavaScript(script)
const state = (target) => evaluate(target, 'window.api.getBootstrap().then(x => x.state)')
const click = (target, id) =>
  evaluate(target, `document.querySelector('[data-testid="${id}"]').click()`)
const has = (target, id) =>
  evaluate(target, `Boolean(document.querySelector('[data-testid="${id}"]'))`)
const latestSequence = (target) =>
  evaluate(
    target,
    'Number(document.querySelector("[data-testid=trace-frame] .frame-sequence")?.textContent ?? 0)'
  )
const traceRows = (target) =>
  evaluate(
    target,
    'Array.from(document.querySelectorAll("[data-testid=trace-frame]")).map(row => row.textContent)'
  )
const appViews = (workspace) =>
  workspace.contentView.children.filter(
    (view) => view instanceof WebContentsView && view.webContents !== workspace.webContents
  )
async function openMenu(workspace, name) {
  const count = menuPopups.length
  await click(workspace, 'menu-' + name)
  await waitFor(() => menuPopups.length > count, 'native ' + name + ' menu')
  const popup = menuPopups.at(-1)
  assert.equal(popup.options.window, workspace)
  return popup
}
function chooseMenu(popup, id, workspace) {
  const item = popup.menu.getMenuItemById(id)
  assert(item?.enabled, 'Menu command must be enabled: ' + id)
  item.click(item, workspace, {})
  popup.options.callback()
}
async function checkReadability(target) {
  const colors = await evaluate(
    target,
    `(() => {
    const css = getComputedStyle(document.documentElement)
    return ['--text', '--muted', '--surface-soft'].map(name => css.getPropertyValue(name).trim())
  })()`
  )
  const luminance = (color) => {
    const channels = color
      .slice(1)
      .match(/../g)
      .map((hex) => {
        const value = parseInt(hex, 16) / 255
        return value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4
      })
    return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722
  }
  for (const text of colors.slice(0, 2)) {
    const values = [luminance(text), luminance(colors[2])].sort((left, right) => right - left)
    assert(
      (values[0] + 0.05) / (values[1] + 0.05) >= 4.5,
      'Text must remain readable in both themes'
    )
  }
}
async function input(target, id, value) {
  await evaluate(
    target,
    `(() => { const input = document.querySelector('[data-testid="${id}"]'); input.value = ${JSON.stringify(value)}; input.dispatchEvent(new Event('input', { bubbles: true })) })()`
  )
}
async function capture(target, name) {
  console.log('Capturing ' + name)
  target.webContents.invalidate()
  await evaluate(
    target,
    'Promise.race([new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))), new Promise(resolve => setTimeout(resolve, 250))])'
  )
  await delay(100)
  let image = await captureSurface(target.webContents)
  assert(!image.isEmpty(), 'Screenshot must contain rendered content')
  if (target instanceof BrowserWindow) {
    // capturePage captures one WebContents. Composite native child surfaces at their real bounds.
    const size = image.getSize()
    const bitmap = image.toBitmap()
    const scale = size.width / target.getContentSize()[0]
    for (const view of appViews(target).filter((entry) => entry.getVisible())) {
      const bounds = view.getBounds()
      const child = await captureSurface(view.webContents)
      const width = Math.round(bounds.width * scale)
      const height = Math.round(bounds.height * scale)
      const pixels = child.resize({ width, height }).toBitmap()
      const x = Math.round(bounds.x * scale)
      const y = Math.round(bounds.y * scale)
      for (let row = 0; row < height; row++) {
        pixels.copy(
          bitmap,
          ((y + row) * size.width + x) * 4,
          row * width * 4,
          (row + 1) * width * 4
        )
      }
    }
    image = nativeImage.createFromBitmap(bitmap, size)
  }
  fs.writeFileSync(path.join(screenshots, name + '.png'), image.toPNG())
}
async function captureSurface(contents) {
  const started = Date.now()
  while (true) {
    try {
      return await contents.capturePage(undefined, { stayAwake: true })
    } catch (error) {
      if (Date.now() - started > 5000) throw error
      contents.invalidate()
      await delay(100)
    }
  }
}
async function assertBounds(workspace, view) {
  await waitFor(async () => {
    const bounds = await evaluate(
      workspace,
      '(() => { const r = document.querySelector("[data-testid=editor-host]").getBoundingClientRect(); return { x: Math.round(r.x), y: Math.round(r.y), width: Math.round(r.width), height: Math.round(r.height) } })()'
    )
    return JSON.stringify(bounds) === JSON.stringify(view.getBounds()) && view.getVisible()
  }, 'native view follows editor bounds')
}

async function run() {
  let exitCode = 1
  const contentsToClose = []
  try {
    await import(pathToFileURL(path.join(root, 'out/main/index.js')).href)
    await app.whenReady()
    await waitFor(() => BrowserWindow.getAllWindows().length === 1, 'workspace creation')
    let workspace = BrowserWindow.getAllWindows()[0]
    await waitFor(() => has(workspace, 'home-page'), 'Home rendering')
    assert.deepEqual(
      await evaluate(
        workspace,
        'Array.from(document.querySelectorAll(".menu-bar button")).map(button => button.textContent)'
      ),
      ['File', 'Edit', 'View', 'Help']
    )
    await checkReadability(workspace)
    assert.equal((await state(workspace)).instances.length, 0)
    assert.equal(
      await evaluate(workspace, 'document.querySelector(".sidebar").innerText.trim()'),
      ''
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector(".sidebar").getBoundingClientRect().width <= 80'
      ),
      true
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=nav-home]").getAttribute("aria-label")'
      ),
      'Home'
    )
    assert.deepEqual(
      await evaluate(
        workspace,
        'Array.from(document.querySelectorAll(".primary-nav button")).map(button => button.getAttribute("aria-label"))'
      ),
      ['Home', 'Logs', 'Apps', 'Docs'],
      'Sidebar navigation follows the requested order and names'
    )
    assert.equal(
      await evaluate(
        workspace,
        'Array.from(document.querySelectorAll(".primary-nav button, .settings-nav")).every(button => button.getBoundingClientRect().height === 40 && button.querySelector("svg").getAttribute("width") === "20")'
      ),
      true,
      'All sidebar buttons and icons use the compact dimensions'
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=nav-logs]").getAttribute("aria-label")'
      ),
      'Logs'
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=nav-settings]").getBoundingClientRect().bottom > innerHeight - 70'
      ),
      true
    )
    await capture(workspace, 'home-empty')

    await click(workspace, 'nav-logs')
    await waitFor(() => has(workspace, 'log-entry'), 'session startup logs')
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=nav-logs]").getAttribute("aria-current")'
      ),
      'page'
    )
    assert(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=logs-page]").textContent.includes("VNet Studio started.")'
      )
    )
    await capture(workspace, 'logs-light')
    await click(workspace, 'nav-home')

    await click(workspace, 'nav-home')
    await waitFor(
      async () => !(await has(workspace, 'instance-explorer')),
      'Home hides its instance list'
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=nav-home]").getAttribute("aria-expanded")'
      ),
      'false'
    )
    await click(workspace, 'nav-home')
    await waitFor(() => has(workspace, 'instance-explorer'), 'Home shows its instance list')
    const fileMenu = await openMenu(workspace, 'file')
    assert(!fileMenu.menu.getMenuItemById('close-application').enabled)
    chooseMenu(fileMenu, 'new-application', workspace)

    await waitFor(() => has(workspace, 'launch-demo'), 'application library')
    await evaluate(workspace, 'window.dispatchEvent(new KeyboardEvent("keydown", { key: "/" }))')
    assert.equal(await evaluate(workspace, 'document.activeElement.dataset.testid'), 'app-search')
    await evaluate(workspace, 'document.activeElement.blur()')
    await capture(workspace, 'apps')
    await click(workspace, 'launch-demo')
    await waitFor(
      async () =>
        (await state(workspace)).instances.length === 1 && (await has(workspace, 'home-page')),
      'launch opens a Home page'
    )
    const firstInstance = (await state(workspace)).instances[0]
    const first = appViews(workspace)[0]
    const firstContents = first.webContents
    contentsToClose.push(firstContents)
    await waitFor(() => has(first, 'toggle-stream'), 'docked demo startup')
    assert.equal(firstInstance.placement, 'docked')
    assert.equal(BaseWindow.getAllWindows().length, 1, 'Docked pages do not create extra windows')
    assert.equal(
      (await evaluate(first, 'window.api.getBootstrap()')).context.instance.id,
      firstInstance.id
    )
    assert.equal(await evaluate(first, 'Boolean(document.querySelector(".demo-caption"))'), false)
    await assertBounds(workspace, first)
    assert.equal(
      await evaluate(
        first,
        'document.querySelector(".trace-footer").getBoundingClientRect().bottom <= document.querySelector(".trace-panel").getBoundingClientRect().bottom'
      ),
      true,
      'Readable trace text must fit inside its panel'
    )
    assert.equal(
      await evaluate(
        first,
        'parseFloat(getComputedStyle(document.querySelector(".trace-table")).fontSize) >= 12'
      ),
      true
    )
    assert.equal(
      await evaluate(
        workspace,
        'parseFloat(getComputedStyle(document.querySelector("[role=tab]")).fontSize) >= 12'
      ),
      true
    )
    await capture(first, 'demo-ready')
    await click(first, 'toggle-stream')
    await delay(1100)
    await click(first, 'toggle-stream')
    const firstCount = await latestSequence(first)
    assert(firstCount >= 2)
    await input(first, 'trace-filter', '0x123')
    const beforeShortcut = menuPopups.length
    first.webContents.sendInputEvent({ type: 'keyDown', keyCode: 'f', modifiers: ['alt'] })
    await waitFor(() => menuPopups.length > beforeShortcut, 'File menu shortcut from a docked app')
    const shortcutMenu = menuPopups.at(-1)
    assert(shortcutMenu.menu.getMenuItemById('new-application'))
    shortcutMenu.options.callback()
    await evaluate(first, 'document.querySelector("[data-testid=trace-filter]").focus()')
    const editMenu = await openMenu(workspace, 'edit')
    chooseMenu(editMenu, 'select-all', workspace)
    await waitFor(
      () =>
        evaluate(
          first,
          '(() => { const input = document.querySelector("[data-testid=trace-filter]"); return input.selectionStart === 0 && input.selectionEnd === input.value.length })()'
        ),
      'Edit menu targets the docked application'
    )
    await capture(first, 'demo-trace')
    const firstRows = await traceRows(first)

    first.webContents.sendInputEvent({ type: 'keyDown', keyCode: '2', modifiers: ['control'] })
    await waitFor(() => has(workspace, 'logs-page'), 'Logs shortcut from a docked application')
    await waitFor(() => !first.getVisible(), 'Logs hides the docked surface')
    assert.equal((await state(workspace)).instances.length, 1)
    await click(workspace, 'nav-home')
    await assertBounds(workspace, first)
    assert.deepEqual(
      await traceRows(first),
      firstRows,
      'Opening Logs preserves the instance session'
    )

    await click(workspace, 'nav-apps')
    await waitFor(() => !first.getVisible(), 'application page hides outside Home')
    await waitFor(
      () => evaluate(workspace, '!document.querySelector("[data-testid=launch-demo]").disabled'),
      'launch button readiness'
    )
    await click(workspace, 'launch-demo')
    await waitFor(
      async () => (await state(workspace)).instances.length === 2,
      'second instance launch'
    )
    const second = appViews(workspace).find((view) => view !== first)
    const secondContents = second.webContents
    contentsToClose.push(secondContents)
    await waitFor(() => has(second, 'toggle-stream'), 'second docked demo')
    assert.equal(await latestSequence(second), 0, 'Instances have independent application state')
    assert.equal(await evaluate(workspace, 'document.querySelectorAll("[role=tab]").length'), 2)
    await assertBounds(workspace, second)
    assert(!first.getVisible())

    const categoryName = (await state(workspace)).applications[0].category
    assert.equal(
      await evaluate(workspace, 'document.querySelectorAll("[data-category]").length'),
      1
    )
    assert.equal(
      await evaluate(workspace, 'document.querySelector("[data-category]").dataset.category'),
      categoryName
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-category] .count-pill").textContent'
      ),
      '2'
    )
    const instanceBeforeCollapse = second.webContents.id
    await click(workspace, 'category-toggle')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=category-toggle]").getAttribute("aria-expanded") === "false" && document.querySelectorAll(".explorer-instance").length === 0'
        ),
      'category collapses its instance rows'
    )
    assert.equal(second.webContents.id, instanceBeforeCollapse)
    assert(second.getVisible(), 'Collapsing a category must not hide its active application page')
    assert.equal((await state(workspace)).instances.length, 2)
    await click(workspace, 'nav-apps')
    await click(workspace, 'nav-home')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=category-toggle]").getAttribute("aria-expanded") === "false"'
        ),
      'category remains collapsed after workspace navigation'
    )
    await click(workspace, 'category-toggle')
    await waitFor(
      () => evaluate(workspace, 'document.querySelectorAll(".explorer-instance").length === 2'),
      'category expands its instances'
    )
    await assertBounds(workspace, second)

    await evaluate(workspace, `window.api.focusInstance(${JSON.stringify(firstInstance.id)})`)
    await assertBounds(workspace, first)
    assert(!second.getVisible())
    assert.deepEqual(await traceRows(first), firstRows, 'Switching tabs preserves trace state')
    const widthWithExplorer = first.getBounds().width
    await click(workspace, 'nav-home')
    await waitFor(
      async () => !(await has(workspace, 'instance-explorer')),
      'instance list collapses with an active app'
    )
    await assertBounds(workspace, first)
    assert(first.getBounds().width > widthWithExplorer)
    assert.deepEqual(
      await traceRows(first),
      firstRows,
      'Collapsing navigation preserves the app instance'
    )
    await capture(workspace, 'home-wide')
    const viewMenu = await openMenu(workspace, 'view')
    assert(!viewMenu.menu.getMenuItemById('instance-list').checked)
    chooseMenu(viewMenu, 'instance-list', workspace)
    await waitFor(() => has(workspace, 'instance-explorer'), 'View menu restores the instance list')
    await assertBounds(workspace, first)
    assert.equal(first.getBounds().width, widthWithExplorer)
    await capture(workspace, 'home-running')
    await capture(first, 'home-editor-content')
    chooseMenu(await openMenu(workspace, 'view'), 'detach-application', workspace)
    await waitFor(
      async () =>
        (await state(workspace)).instances.find((instance) => instance.id === firstInstance.id)
          .placement === 'detached',
      'tab detached into window'
    )
    const floating = BaseWindow.getAllWindows().find((window) => window !== workspace)
    assert(floating.contentView.children.includes(first), 'Detach moves the original view')
    assert.deepEqual(await traceRows(first), firstRows)
    assert.equal(
      await evaluate(first, 'document.querySelector("[data-testid=trace-filter]").value'),
      '0x123'
    )
    await waitFor(
      () => evaluate(first, 'Boolean(document.querySelector(".demo-caption"))'),
      'detached window caption'
    )
    await capture(first, 'demo-detached')
    await evaluate(workspace, `window.api.focusInstance(${JSON.stringify(firstInstance.id)})`)
    assert(focusedWindows.has(floating.id))
    await click(first, 'change-placement')
    await waitFor(
      async () =>
        (await state(workspace)).instances.find((instance) => instance.id === firstInstance.id)
          .placement === 'docked',
      'window re-docked to Home'
    )
    await assertBounds(workspace, first)
    assert.equal(BaseWindow.getAllWindows().length, 1)
    assert(appViews(workspace).includes(first), 'Re-docking retains the same view and renderer')
    assert.deepEqual(await traceRows(first), firstRows)
    assert.equal(
      await evaluate(first, 'document.querySelector("[data-testid=trace-filter]").value'),
      '0x123'
    )
    await input(first, 'trace-filter', 'not-a-message')
    await waitFor(async () => !(await has(first, 'trace-frame')), 'trace filter')
    await input(first, 'trace-filter', '')
    await waitFor(() => has(first, 'trace-frame'), 'filter clearing')
    await click(first, 'clear-trace')
    assert.equal(await latestSequence(first), 0)
    await click(first, 'toggle-stream')
    first.webContents.sendInputEvent({ type: 'keyDown', keyCode: '4', modifiers: ['control'] })
    await waitFor(() => has(workspace, 'docs-page'), 'Docs shortcut from an application view')
    const countBeforeHide = await latestSequence(first)
    await waitFor(
      async () => (await latestSequence(first)) > countBeforeHide,
      'background instance continues running'
    )
    await click(workspace, 'nav-home')
    await assertBounds(workspace, first)
    await click(first, 'toggle-stream')

    workspace.setSize(960, 720)
    await assertBounds(workspace, first)
    assert.equal(
      await evaluate(workspace, 'document.documentElement.scrollWidth <= innerWidth'),
      true
    )
    await capture(workspace, 'home-compact')
    await capture(first, 'home-compact-content')
    workspace.setSize(1280, 840)
    await assertBounds(workspace, first)
    await click(workspace, 'nav-settings')
    await waitFor(() => !first.getVisible(), 'settings hides docked application')
    await click(workspace, 'theme-dark')
    await waitFor(
      () => evaluate(first, 'document.documentElement.dataset.theme === "dark"'),
      'application theme'
    )
    await checkReadability(workspace)
    await checkReadability(first)
    await capture(workspace, 'settings-dark')
    assert.equal(
      JSON.parse(fs.readFileSync(path.join(profile, 'workspace-preferences.json'), 'utf8')).settings
        .theme,
      'dark'
    )
    await click(workspace, 'close-apps-on-exit')
    await waitFor(async () => !(await state(workspace)).settings.closeAppsOnExit, 'exit preference')
    workspace.close()
    await waitFor(
      () => BaseWindow.getAllWindows().length === 2 && BrowserWindow.getAllWindows().length === 0,
      'docked apps moved to windows when workspace closes'
    )
    assert(!firstContents.isDestroyed() && !secondContents.isDestroyed())
    await waitFor(() => has(first, 'show-workspace'), 'independent app navigation')
    await click(first, 'show-workspace')
    await waitFor(() => BrowserWindow.getAllWindows().length === 1, 'workspace reopens')
    workspace = BrowserWindow.getAllWindows()[0]
    await waitFor(() => has(workspace, 'home-page'), 'reopened Home')
    assert.equal(
      (await state(workspace)).instances.filter((instance) => instance.placement === 'detached')
        .length,
      2
    )
    await evaluate(workspace, `window.api.dockInstance(${JSON.stringify(firstInstance.id)})`)
    await assertBounds(workspace, first)
    assert.equal(BaseWindow.getAllWindows().length, 2)
    const secondWindow = BaseWindow.getAllWindows().find((window) => window !== workspace)
    secondWindow.close()
    await waitFor(
      async () => (await state(workspace)).instances.length === 1 && secondContents.isDestroyed(),
      'closing an external window releases its instance'
    )

    await click(workspace, 'nav-apps')
    await click(workspace, 'app-enabled')
    await waitFor(
      () => evaluate(workspace, 'document.querySelector("[data-testid=launch-demo]").disabled'),
      'application disabled'
    )
    console.log('Checking disabled application launch rejection (expected error below).')
    assert.equal(
      await evaluate(
        workspace,
        'window.api.launchApplication("bus-trace-demo").then(() => false, () => true)'
      ),
      true
    )
    assert.equal((await state(workspace)).instances.length, 1)
    await click(workspace, 'app-enabled')
    await waitFor(
      () => evaluate(workspace, '!document.querySelector("[data-testid=launch-demo]").disabled'),
      'application enabled'
    )
    await input(workspace, 'app-search', 'not-an-application')
    await waitFor(async () => !(await has(workspace, 'launch-demo')), 'application search')
    await input(workspace, 'app-search', '')
    await waitFor(() => has(workspace, 'launch-demo'), 'search clearing')
    chooseMenu(await openMenu(workspace, 'view'), 'logs', workspace)
    await waitFor(() => has(workspace, 'logs-page'), 'View menu opens Logs')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=logs-page]").textContent.includes("This application is unavailable.")'
        ),
      'failed workspace action is recorded'
    )
    await capture(workspace, 'logs-runtime-dark')
    await input(workspace, 'log-search', 'bus-trace-demo')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelectorAll("[data-testid=log-entry]").length > 0 && Array.from(document.querySelectorAll("[data-testid=log-entry]")).every(row => row.textContent.includes("bus-trace-demo"))'
        ),
      'logs filter by message'
    )
    await input(workspace, 'log-search', 'not-a-runtime-message')
    await waitFor(async () => !(await has(workspace, 'log-entry')), 'log search empty result')
    await input(workspace, 'log-search', '')
    await evaluate(
      workspace,
      '(() => { const select = document.querySelector("[data-testid=log-level]"); select.value = "error"; select.dispatchEvent(new Event("change", { bubbles: true })) })()'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelectorAll("[data-testid=log-entry]").length > 0 && Array.from(document.querySelectorAll("[data-testid=log-entry] .log-level")).every(level => level.textContent === "error")'
        ),
      'log severity filter'
    )
    await click(workspace, 'clear-logs')
    await waitFor(
      async () =>
        (await evaluate(workspace, 'window.api.getLogs()')).entries.length === 0 &&
        !(await has(workspace, 'log-entry')),
      'clear session logs'
    )
    await evaluate(
      workspace,
      '(() => { const select = document.querySelector("[data-testid=log-level]"); select.value = "warning"; select.dispatchEvent(new Event("change", { bubbles: true })) })()'
    )
    await evaluate(first, 'console.warn("Trace warning <b>plain text</b>")')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=log-entry] .log-message")?.textContent === "Trace warning <b>plain text</b>"'
        ),
      'live renderer warning is captured after clearing'
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=log-entry] .log-source").textContent'
      ),
      'Bus Trace Demo 01'
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=log-entry] .log-message").children.length'
      ),
      0,
      'Log messages render as plain text'
    )
    await click(workspace, 'nav-apps')
    await evaluate(first, 'console.error("Trace error while Logs is closed")')
    await evaluate(
      workspace,
      'document.activeElement.blur(); window.dispatchEvent(new KeyboardEvent("keydown", { key: "2", ctrlKey: true }))'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=logs-page]")?.textContent.includes("Trace error while Logs is closed")'
        ),
      'Logs shortcut reloads records collected while the viewer was closed'
    )
    chooseMenu(await openMenu(workspace, 'help'), 'documentation', workspace)
    await waitFor(() => has(workspace, 'docs-page'), 'Help menu opens Docs')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Getting started"'
        ),
      'bundled main program manual'
    )
    assert.equal(
      await evaluate(workspace, 'document.querySelectorAll("[data-guide-id]").length'),
      (await state(workspace)).applications.length + 1
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelector("[data-testid=help-content] table tbody").rows.length'
      ),
      5
    )
    await capture(workspace, 'help-main-dark')
    await click(workspace, 'help-guide-application-bus-trace-demo')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-guide-application-bus-trace-demo]").getAttribute("aria-expanded") === "false"'
        ),
      'application guide collapses'
    )
    await click(workspace, 'help-guide-application-bus-trace-demo')
    await evaluate(
      workspace,
      'document.querySelector("[data-chapter-id=\\"application-bus-trace-demo/message-trace\\"]").click()'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Message trace"'
        ),
      'application chapter selection'
    )
    const readerUrl = workspace.webContents.getURL()
    await evaluate(
      workspace,
      'document.querySelector("[data-testid=help-content] a[href=\\"filtering.md\\"]").click()'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Filtering messages"'
        ),
      'relative chapter link'
    )
    assert.equal(
      await evaluate(
        workspace,
        'Boolean(document.querySelector("[data-testid=help-content] pre code"))'
      ),
      true
    )
    assert.equal(
      await evaluate(
        workspace,
        'document.querySelectorAll("[data-testid=help-content] textarea, [data-testid=help-content] [contenteditable=true]").length'
      ),
      0
    )
    await input(workspace, 'help-search', 'filtering')
    await waitFor(
      () => evaluate(workspace, 'document.querySelectorAll("[data-chapter-id]").length === 1'),
      'chapter search'
    )
    await input(workspace, 'help-search', '')
    await evaluate(
      workspace,
      'document.querySelector("[data-chapter-id=\\"workspace/settings\\"]").click()'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Preferences"'
        ),
      'main program preferences chapter'
    )
    await evaluate(
      workspace,
      'document.querySelector("[data-testid=help-content] a[href=\\"instances.md#close-the-workspace\\"]").click()'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Tabs and windows" && document.querySelector("[data-testid=help-document-scroll]").scrollTop > 0'
        ),
      'chapter heading anchor'
    )
    assert.equal(
      workspace.webContents.getURL(),
      readerUrl,
      'Documentation links stay inside the reader'
    )
    await evaluate(
      workspace,
      'document.querySelector("[data-chapter-id=\\"application-bus-trace-demo/overview\\"]").click()'
    )
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Bus Trace Demo"'
        ),
      'application manual overview'
    )
    await evaluate(
      workspace,
      'document.activeElement.blur(); window.dispatchEvent(new KeyboardEvent("keydown", { key: "4", ctrlKey: true }))'
    )
    await waitFor(() => has(workspace, 'docs-page'), 'Docs shortcut')
    await click(workspace, 'nav-home')
    await click(workspace, 'close-tab')
    await waitFor(
      async () => (await state(workspace)).instances.length === 0 && firstContents.isDestroyed(),
      'closing a tab releases its instance'
    )
    await waitFor(() => has(workspace, 'browse-apps'), 'editor welcome restored')
    await click(workspace, 'nav-settings')
    await click(workspace, 'theme-light')
    await waitFor(
      () => evaluate(workspace, 'document.documentElement.dataset.theme === "light"'),
      'light theme restored'
    )
    await click(workspace, 'close-apps-on-exit')
    await waitFor(
      async () => (await state(workspace)).settings.closeAppsOnExit,
      'exit preference restored'
    )
    await click(workspace, 'nav-docs')
    await waitFor(
      () =>
        evaluate(
          workspace,
          'document.querySelector("[data-testid=help-content] h1")?.textContent === "Bus Trace Demo"'
        ),
      'selected manual survives navigation'
    )
    await waitFor(
      () => evaluate(workspace, '!document.querySelector(".toast-message")'),
      'documentation view settles before capture'
    )
    await capture(workspace, 'help-documents')
    workspace.setSize(960, 720)
    await delay(150)
    assert.equal(
      await evaluate(workspace, 'document.documentElement.scrollWidth <= innerWidth'),
      true
    )
    await capture(workspace, 'help-compact')
    workspace.setSize(1280, 840)
    await click(workspace, 'nav-apps')
    await click(workspace, 'launch-demo')
    await waitFor(
      async () =>
        (await state(workspace)).instances.length === 1 && appViews(workspace).length === 1,
      'final instance launch'
    )
    const finalView = appViews(workspace)[0]
    const finalContents = finalView.webContents
    contentsToClose.push(finalContents)
    await waitFor(() => has(finalView, 'toggle-stream'), 'final instance ready')
    workspace.close()
    await waitFor(
      () => BaseWindow.getAllWindows().length === 0 && finalContents.isDestroyed(),
      'workspace exit closes all application contents'
    )
    console.log(
      'Readability, compact navigation, runtime logs, native menus, categories, editor tabs, docking, trace, settings, and shutdown: OK'
    )
    console.log('Screenshots: ' + screenshots)
    exitCode = 0
  } catch (error) {
    console.error(error)
  } finally {
    clearTimeout(deadline)
    for (const window of BaseWindow.getAllWindows()) window.destroy()
    for (const contents of contentsToClose) if (!contents.isDestroyed()) contents.close()
    app.removeListener('will-quit', holdQuit)
    app.exit(exitCode)
  }
}
void run()
