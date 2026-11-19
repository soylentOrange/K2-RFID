// SPDX-License-Identifier: GPL-3.0-or-later
// Firmware installer (GitHub Pages only): flashes the factory image of a release
// with ESP Web Tools. firmware/index.json and the manifests are created by
// tools/fetch-firmware.mjs when the Pages site is built.
import 'esp-web-tools/dist/install-button.js'
import './style.css'
import { fitToScreen } from './fit.js'

const $ = (id) => document.getElementById(id)
const params = new URLSearchParams(window.location.search)
let releases = []

function selectedBuild() {
  const release = releases.find((r) => r.tag === $('release').value)
  return release?.builds.find((b) => !b.staticStack && b.board === $('board').value)
}

function update() {
  const release = releases.find((r) => r.tag === $('release').value)
  const build = selectedBuild()
  $('install-button').disabled = !build
  if (build) $('install').setAttribute('manifest', `firmware/${build.manifest}`)
  else $('install').removeAttribute('manifest')
  $('install-info').textContent = build
    ? `${build.label} (${build.chipFamily}), ${release.tag}`
    : 'No firmware for this combination.'
  // the S2 (native USB only) needs the buttons for entering download mode and for the reset
  const s2 = build ? build.chipFamily === 'ESP32-S2' : true
  for (const el of document.querySelectorAll('.s2-only')) el.hidden = !s2
  if (release) $('release-link').href = release.url
}

async function init() {
  try {
    const res = await fetch('firmware/index.json', { cache: 'no-store' })
    if (!res.ok) throw new Error(res.statusText)
    releases = (await res.json()).releases
  } catch (e) {
    $('install-info').textContent = `No firmware available (${e.message}).`
    return
  }

  for (const r of releases) {
    $('release').append(new Option(`${r.tag}${r.prerelease ? ' (pre-release)' : ''}`, r.tag))
  }
  const boards = new Map()
  for (const r of releases) for (const b of r.builds) boards.set(b.board, b.label)
  for (const [id, label] of boards) $('board').append(new Option(label, id))

  // ?board=lolin_s2_mini&release=v1.4.1-beta preselects (e.g. for testing a pre-release)
  if (params.get('board') && boards.has(params.get('board'))) $('board').value = params.get('board')
  if (releases.some((r) => r.tag === params.get('release'))) $('release').value = params.get('release')

  for (const id of ['board', 'release']) $(id).addEventListener('change', update)
  update()
}

fitToScreen(document.querySelector('.app'))
init()

// The thingy logs via the web log (WebSerial in the web app), not over USB, so the
// "Logs & Console" entry of the ESP Web Tools dialog would only show an empty console.
// ESP Web Tools has no option for it: hide the entry whenever the dialog renders it.
function hideLogsEntry(dialog) {
  const hide = () => {
    for (const item of dialog.shadowRoot?.querySelectorAll('ew-list-item') ?? []) {
      if (item.textContent.includes('Logs & Console')) item.style.display = 'none'
    }
  }
  hide()
  new MutationObserver(hide).observe(dialog.shadowRoot, { childList: true, subtree: true })
}

new MutationObserver((mutations) => {
  for (const m of mutations) {
    for (const node of m.addedNodes) {
      if (node.localName === 'ewt-install-dialog') node.updateComplete.then(() => hideLogsEntry(node))
    }
  }
}).observe(document.body, { childList: true })
