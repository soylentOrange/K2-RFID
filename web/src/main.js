// SPDX-License-Identifier: GPL-3.0-or-later
// K2-RFID web app: select spool material, arm the programmer, manage the thingy.

import './style.css'
import { Device } from './device.js'
import { MaterialDb, gzip } from './materials.js'
import { findPrinter, downloadPrinterDb } from './printer.js'
import { load, save } from './storage.js'
import { toast } from './toast.js'
import { successIcon, errorIcon, tagIcon, spoolIcon } from './icons.js'

const $ = (id) => document.getElementById(id)

const DEFAULT_SPOOL = { type: '00001', weight: 1000, color: '#000000' }
const DEFAULT_HOST = 'k2rfid.local'

const device = new Device()
const db = new MaterialDb()

// state mirrored from the thingy
const state = {
  clientId: undefined,
  hostIp: undefined,
  writeTags: false,
  writeEmptyTags: true,
  beepOnRW: false,
  beepAvailable: true,
  cloneSerial: false,
  readerAvailable: true,
}

// currently selected spool material (remembered across visits)
let spool = { ...DEFAULT_SPOOL, ...load('spool', {}) }

// ---------------------------------------------------------------------------
// views
// ---------------------------------------------------------------------------

// the views are pages in a row: main <-> settings <-> database
const PAGES = ['main', 'settings', 'database']
const PAGE_TITLES = { main: 'Main', settings: 'Settings', database: 'Database' }
let currentPage = 'main'

function showView(name) {
  for (const view of [...PAGES, 'result']) $(`view-${view}`).hidden = view !== name
  $('nav').hidden = name === 'result'
  if (name === 'result') return

  currentPage = name
  const index = PAGES.indexOf(name)
  for (const [id, target] of [['nav-back', PAGES[index - 1]], ['nav-next', PAGES[index + 1]]]) {
    const button = $(id)
    button.hidden = !target
    if (!target) continue
    button.textContent = id === 'nav-back' ? `<< ${PAGE_TITLES[target]}` : `${PAGE_TITLES[target]} >>`
    button.onclick = () => showView(target)
  }
  updateStatus()
}

// show the progress view while running a long operation, then the outcome
// (and return to the page it was started from, unless returnTo says otherwise)
async function runWithProgress(message, task, { returnTo = currentPage, delay = 5000 } = {}) {
  const icon = $('result-icon')
  const text = $('result-text')
  icon.innerHTML = '<div class="spinner"></div>'
  text.textContent = message
  $('status').textContent = ' '
  showView('result')

  const progress = (msg) => (text.textContent = msg)
  let ok = true
  try {
    text.textContent = (await task(progress)) ?? 'Done'
  } catch (error) {
    ok = false
    text.textContent = error.message
  }
  icon.innerHTML = `<img src="${ok ? successIcon : errorIcon}" alt="${ok ? 'success' : 'error'}" />`
  if (returnTo) setTimeout(() => showView(returnTo), delay)
  return ok
}

function updateStatus() {
  const status = $('status')
  if (!device.connected) status.textContent = device.host ? `Not connected to ${device.host}` : 'Not connected'
  else if (!state.writeTags) status.textContent = 'Waiting to read...'
  else status.textContent = state.writeEmptyTags ? 'Waiting to Write...' : 'Waiting to (Re-)Write...'
}

function setInputsEnabled(enabled) {
  for (const el of document.querySelectorAll('[data-needs-connection]')) el.disabled = !enabled
  if (!state.beepAvailable) $('beep').disabled = true
}

// ---------------------------------------------------------------------------
// spool material
// ---------------------------------------------------------------------------

function fillSelect(select, options) {
  select.replaceChildren(...options.map(([value, text]) => new Option(text, value)))
}

// render the current spool into the form
function renderSpool() {
  $('color').value = spool.color
  $('color-text').textContent = spool.color
  $('weight').value = spool.weight
  $('weight-text').textContent = `${spool.weight} g`

  const brand = db.brandOf(spool.type) ?? load('brand') ?? db.brands[0]
  fillSelect($('brand'), db.brands.map((b) => [b, b]))
  $('brand').value = brand
  renderTypes(brand)
}

function renderTypes(brand) {
  const materials = db.materials(brand)
  fillSelect($('type'), materials.map((m) => [m.id, m.name]))
  if (!materials.some((m) => m.id === spool.type)) spool.type = materials[0]?.id
  $('type').value = spool.type
}

function setSpool(update, { remember = true } = {}) {
  spool = { ...spool, ...update }
  spool.color = String(spool.color).toUpperCase()
  spool.weight = Number(spool.weight) || DEFAULT_SPOOL.weight
  if (remember) save('spool', { type: spool.type, weight: spool.weight, color: spool.color })
  renderSpool()
}

// ---------------------------------------------------------------------------
// commands to the thingy
// ---------------------------------------------------------------------------

function arm() {
  const { type, weight, color, serial } = spool
  device.send({
    type: 'arm_state',
    writeTags: true,
    writeEmptyTags: state.writeEmptyTags,
    spooldata: { type, weight, color, serial: state.cloneSerial ? serial : null },
  })
}

function disarm() {
  device.send({ type: 'arm_state', writeTags: false, writeEmptyTags: state.writeEmptyTags })
}

function sendConfig() {
  device.send({ type: 'update_config', beepOnRW: state.beepOnRW, cloneSerial: state.cloneSerial })
}

async function loadDbFromThingy({ silent = false } = {}) {
  try {
    const response = await device.fetch('/material_database.json')
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    db.load(await response.json())
    renderSpool()
    $('db-info').textContent = `${db.count} materials (version ${db.version})`
    if (!silent) toast('Material Database\nloaded from thingy', { icon: successIcon })
  } catch (error) {
    toast(`Material Database\n${error.message}`, { icon: errorIcon })
  }
}

// validate, compress and upload a material database to the thingy
async function uploadDb(text) {
  let json
  try {
    json = JSON.parse(text)
  } catch {
    throw new Error('Not a valid JSON file...')
  }
  new MaterialDb().load(json) // throws if it's not a material database
  const form = new FormData()
  form.append('dbfile', await gzip(text), 'material_database.json.gz')
  const response = await device.fetch('/material_database.json', { method: 'POST', body: form })
  const result = await response.text()
  if (!response.ok || result !== 'OK') throw new Error(`Upload failed: ${result || response.status}`)
}

// ---------------------------------------------------------------------------
// printer (source of the material database)
// ---------------------------------------------------------------------------

const FETCH_DB_TIMEOUT = 60000
let onFetchDbResult = null // set while the thingy downloads the database

async function loadPrinterSettings() {
  try {
    const response = await device.fetch('/api/printer')
    if (!response.ok) return // older firmware without printer settings
    const printer = await response.json()
    $('printer-host').value = printer.host
    $('printer-password').placeholder = printer.customPassword ? 'root password (saved)' : 'root password (default)'
  } catch {
    // not reachable - nothing to show
  }
}

// store printer address (and password, if one was entered) on the thingy
async function savePrinter() {
  const body = new URLSearchParams({ host: $('printer-host').value.trim() })
  if ($('printer-password').value) body.set('password', $('printer-password').value)
  await device.post('/api/printer', body, 'application/x-www-form-urlencoded')
  $('printer-password').value = ''
  await loadPrinterSettings()
}

// scan the local network for a printer and remember it
async function searchPrinter() {
  const printer = await findPrinter(state.hostIp ?? device.host)
  $('printer-host').value = printer.ip
  await savePrinter()
  return printer
}

// let the thingy download the database via SSH; resolves with its report
function fetchDbViaThingy() {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => finish({ result: false, message: 'No answer from the thingy' }), FETCH_DB_TIMEOUT)
    const finish = (msg) => {
      clearTimeout(timeout)
      onFetchDbResult = null
      msg.result ? resolve(msg.message) : reject(new Error(msg.message))
    }
    onFetchDbResult = finish
    device.post('/api/fetchdb').catch((error) => finish({ result: false, message: error.message }))
  })
}

async function updateDbFromPrinter(progress) {
  if ($('printer-host').value.trim()) {
    await savePrinter()
  } else {
    progress('Looking for a printer on the local network...')
    const printer = await searchPrinter()
    progress(`Found ${printer.model} at ${printer.ip}`)
  }
  const host = $('printer-host').value.trim()
  progress(`Downloading the material database from ${host} (via SSH)...`)
  try {
    return await fetchDbViaThingy()
  } catch (sshError) {
    // older printer firmware still serves its default database via http
    try {
      await uploadDb(await downloadPrinterDb(host))
      return `Fetched the database from ${host} (via http) and uploaded it to the thingy.`
    } catch {
      throw sshError
    }
  }
}

// ---------------------------------------------------------------------------
// messages from the thingy
// ---------------------------------------------------------------------------

function onConfig(msg) {
  state.beepOnRW = msg.beepOnRW
  state.beepAvailable = msg.beepAvailable
  state.cloneSerial = msg.cloneSerial
  $('beep').checked = state.beepOnRW
  $('beep').disabled = !state.beepAvailable
  $('clone').checked = state.cloneSerial
}

function onArmState(msg, { initial = false } = {}) {
  const changed = state.writeTags !== msg.writeTags
  state.writeTags = msg.writeTags
  state.writeEmptyTags = msg.writeEmptyTags
  $('write-empty').checked = state.writeEmptyTags
  updateStatus()
  if (changed || (initial && msg.writeTags)) {
    toast(`${msg.writeTags ? 'Armed' : 'Disarmed'}\n${$('status').textContent}`, { icon: tagIcon })
  }
}

function onSpooldata(msg, { notify = true } = {}) {
  if (!msg.spooldata) return
  setSpool(msg.spooldata, { remember: false })
  if (notify) {
    toast(`Spooldata updated\ncolor: ${msg.spooldata.color}`, { icon: spoolIcon(msg.spooldata.color) })
  }
}

const handlers = {
  initial_config(msg) {
    state.clientId = msg.id
    state.hostIp = msg.host
    onConfig(msg)
    onSpooldata(msg, { notify: false })
    onArmState(msg, { initial: true })
    loadDbFromThingy()
    loadPrinterSettings()
    if (!msg.PN532) {
      state.readerAvailable = false
      runWithProgress('Checking reader...', () => Promise.reject(new Error('Reader not available!')), { returnTo: null })
    }
  },
  arm_state(msg) {
    if (msg.origin !== state.clientId) onSpooldata(msg)
    onArmState(msg)
  },
  update_config(msg) {
    if (msg.origin !== state.clientId) onConfig(msg)
  },
  new_db() {
    loadDbFromThingy()
  },
  fetch_db(msg) {
    if (onFetchDbResult) onFetchDbResult(msg)
    else toast(`Material Database\n${msg.message}`, { icon: msg.result ? successIcon : errorIcon })
  },
  read_tag(msg) {
    toast(`Found empty tag\nuid: ${String(msg.uid).toUpperCase()}`, { icon: tagIcon })
  },
  read_spool(msg) {
    const data = msg.spooldata
    toast(`Found spool\nserial: ${String(data.serial).padStart(6, '0')}\ncolor: ${data.color}\n(tap to copy)`, {
      icon: spoolIcon(data.color),
      duration: 5000,
      onClick: () => {
        setSpool(data, { remember: false })
        disarm()
      },
    })
  },
  write_spool(msg) {
    toast(`Writing Spool\n${msg.result ? 'Successful' : 'Failed'}`, { icon: msg.result ? successIcon : errorIcon })
  },
}

// ---------------------------------------------------------------------------
// wiring
// ---------------------------------------------------------------------------

function bindUi() {
  // log of the thingy (hex dumps of the tags read, ...)
  $('open-weblog').onclick = () => window.open(device.url('/weblog'), '_blank', 'noopener')

  $('color').oninput = (e) => ($('color-text').textContent = e.target.value.toUpperCase())
  $('color').onchange = (e) => setSpool({ color: e.target.value })
  $('weight').oninput = (e) => ($('weight-text').textContent = `${e.target.value} g`)
  $('weight').onchange = (e) => setSpool({ weight: e.target.value })
  $('brand').onchange = (e) => {
    save('brand', e.target.value)
    renderTypes(e.target.value)
    setSpool({ type: $('type').value })
  }
  $('type').onchange = (e) => setSpool({ type: e.target.value })

  $('reset').onclick = () => {
    save('brand', null)
    setSpool({ ...DEFAULT_SPOOL, serial: undefined })
    disarm()
  }
  $('apply').onclick = arm

  $('beep').onchange = (e) => {
    state.beepOnRW = e.target.checked
    sendConfig()
  }
  $('clone').onchange = (e) => {
    state.cloneSerial = e.target.checked
    sendConfig()
  }
  $('write-empty').onchange = (e) => {
    state.writeEmptyTags = e.target.checked
    state.writeTags ? arm() : disarm()
  }

  $('db-from-printer').onclick = () => runWithProgress('Updating the material database...', updateDbFromPrinter)

  $('printer-search').onclick = () =>
    runWithProgress('Looking for a printer on the local network...', async () => {
      const printer = await searchPrinter()
      return `Found ${printer.model} at ${printer.ip}\n\nSaved as printer for database updates.`
    })
  $('printer-form').onsubmit = (e) => {
    e.preventDefault()
    savePrinter().then(
      () => toast('Printer saved', { icon: successIcon }),
      (error) => toast(`Printer not saved\n${error.message}`, { icon: errorIcon }),
    )
  }

  $('db-from-file').onclick = () => $('db-file').click()
  $('db-file').onchange = (e) => {
    const [file] = e.target.files
    e.target.value = ''
    if (file) {
      runWithProgress(`Uploading ${file.name}...`, async () => {
        await uploadDb(await file.text())
        return `Uploaded ${file.name} to the thingy.`
      })
    }
  }

  // restart / clear wifi / safeboot - then reload, which shows whatever the thingy serves next
  for (const button of document.querySelectorAll('[data-action]')) {
    button.onclick = () =>
      runWithProgress('Processing...', () => device.post(button.dataset.action), { returnTo: null }).then(() =>
        setTimeout(() => window.location.reload(), 7500),
      )
  }

  // GitHub Pages: choose the device to talk to
  $('device-form').onsubmit = (e) => {
    e.preventDefault()
    const host = $('device-host').value.trim().replace(/^https?:\/\//, '').replace(/\/.*$/, '') || DEFAULT_HOST
    save('host', host)
    connect(host)
  }
}

// connect to the thingy ('' = the host that served this page)
function connect(host) {
  if (!__DEVICE_BUILD__) {
    $('device-host').value = host
    $('device-direct-link').href = `http://${host}/`
    $('device-direct-link').textContent = `http://${host}`
  }
  device.connect(host)
  device.text('/boardname').then((t) => ($('info-board').textContent = t), () => {})
  device.text('/appversion').then((t) => ($('info-build').textContent = t), () => {})
}

function bindDevice() {
  device.addEventListener('connecting', () => ($('status').textContent = 'Connecting...'))
  device.addEventListener('open', () => {
    setInputsEnabled(true)
    updateStatus()
  })
  device.addEventListener('close', () => {
    setInputsEnabled(false)
    updateStatus()
  })
  device.addEventListener('timeout', () => toast('Connection lost\nReconnecting...', { icon: errorIcon }))
  device.addEventListener('message', ({ detail: msg }) => handlers[msg.type]?.(msg))
}

function init() {
  $('info-app').textContent = __DEVICE_BUILD__ ? 'embedded' : 'GitHub Pages'
  $('logo').onerror = (e) => e.target.replaceWith(Object.assign(document.createElement('h1'), { textContent: 'K2 RFID Tag Programmer' }))

  bindUi()
  bindDevice()
  renderSpool()
  showView('main')

  if (__DEVICE_BUILD__) {
    // offer the https app (works without opening the device's address)
    const link = $('other-app-link')
    link.href = `${__PAGES_URL__}?device=${encodeURIComponent(window.location.host)}`
    link.textContent = 'Open the web app on GitHub Pages'
    link.hidden = false
    connect('')
  } else {
    $('device-settings').hidden = false
    const fromUrl = new URLSearchParams(window.location.search).get('device')
    const host = fromUrl || load('host', DEFAULT_HOST)
    if (fromUrl) save('host', fromUrl)
    connect(host)
  }
}

init()
