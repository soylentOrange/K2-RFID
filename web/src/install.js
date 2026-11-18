// SPDX-License-Identifier: GPL-3.0-or-later
// Firmware installer (GitHub Pages only): flashes the factory image of a release
// with ESP Web Tools. firmware/index.json and the manifests are created by
// tools/fetch-firmware.mjs when the Pages site is built.
import 'esp-web-tools/dist/install-button.js'
import './style.css'

const $ = (id) => document.getElementById(id)
const params = new URLSearchParams(window.location.search)
let releases = []

function selectedBuild() {
  const release = releases.find((r) => r.tag === $('release').value)
  return release?.builds.find((b) => b.board === $('board').value && b.staticStack === $('static-stack').checked)
}

function update() {
  const release = releases.find((r) => r.tag === $('release').value)
  const build = selectedBuild()
  $('install-button').disabled = !build
  if (build) $('install').setAttribute('manifest', `firmware/${build.manifest}`)
  else $('install').removeAttribute('manifest')
  $('install-info').textContent = build
    ? `${build.label} (${build.chipFamily}), ${release.tag}${build.staticStack ? ', static stack' : ''}`
    : 'No firmware for this combination.'
  $('step-boot').hidden = build ? build.chipFamily !== 'ESP32-S2' : false
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

  for (const id of ['board', 'release', 'static-stack']) $(id).addEventListener('change', update)
  update()
}

init()
