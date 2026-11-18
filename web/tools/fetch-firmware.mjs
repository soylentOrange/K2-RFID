// SPDX-License-Identifier: GPL-3.0-or-later
// Copies the factory images of the newest release (and of a newer pre-release, if any)
// into the GitHub Pages site and writes an ESP Web Tools manifest for each of them.
// Also copies the app images (<board>.firmware.bin) for the firmware update over WiFi
// in the web app (via SafeBoot).
// ESP Web Tools and the web app fetch the images from the browser, and GitHub release
// downloads can't be fetched cross-origin, so the images have to be served from the Pages site.
//
// Usage: node tools/fetch-firmware.mjs [outDir]   (default: dist/firmware)
// Env:   GITHUB_REPOSITORY (owner/repo, default soylentOrange/K2-RFID), GH_TOKEN or GITHUB_TOKEN (optional)
import { mkdir, writeFile } from 'node:fs/promises'
import { join } from 'node:path'

const repo = process.env.GITHUB_REPOSITORY || 'soylentOrange/K2-RFID'
const token = process.env.GH_TOKEN || process.env.GITHUB_TOKEN
const outDir = process.argv[2] || 'dist/firmware'

// release asset name (without .firmware.factory.bin) -> board
const BOARDS = {
  lolin_s2_mini: { label: 'LOLIN S2 Mini', chipFamily: 'ESP32-S2' },
  lolin_s2_mini_external_WS2812b: { label: 'LOLIN S2 Mini + external WS2812B LED', chipFamily: 'ESP32-S2' },
  esp32_s3_tiny: { label: 'Waveshare ESP32-S3-Tiny', chipFamily: 'ESP32-S3' },
}
const SUFFIX = '.firmware.factory.bin'
const APP_SUFFIX = '.firmware.bin'
const STATIC = '_static-stack'

async function api(path) {
  const headers = { Accept: 'application/vnd.github+json', 'User-Agent': 'k2rfid-pages-build' }
  if (token) headers.Authorization = `Bearer ${token}`
  const res = await fetch(`https://api.github.com/repos/${repo}${path}`, { headers })
  if (!res.ok) throw new Error(`GitHub API ${path}: ${res.status} ${res.statusText}`)
  return res.json()
}

async function download(url, file) {
  const res = await fetch(url, { headers: { 'User-Agent': 'k2rfid-pages-build' } })
  if (!res.ok) throw new Error(`Download ${url}: ${res.status} ${res.statusText}`)
  await writeFile(file, Buffer.from(await res.arrayBuffer()))
}

async function publish(release) {
  const tag = release.tag_name
  const version = tag.replace(/^v/, '')
  const dir = join(outDir, tag)
  await mkdir(dir, { recursive: true })
  const builds = []
  for (const asset of release.assets) {
    if (!asset.name.endsWith(SUFFIX)) continue
    const id = asset.name.slice(0, -SUFFIX.length)
    // older releases also have a "_static-stack" variant per board; since v1.5.2 the SSH
    // download runs in PSRAM and the plain build is the right one for everybody
    if (id.endsWith(STATIC)) continue
    const boardId = id
    const board = BOARDS[boardId]
    if (!board) {
      console.warn(`${tag}: skipping unknown board ${asset.name}`)
      continue
    }
    console.log(`${tag}: ${asset.name}`)
    await download(asset.browser_download_url, join(dir, asset.name))
    const manifest = {
      name: `K2-RFID (${board.label})`,
      version,
      // the factory image also overwrites the settings, so offering to erase first is harmless
      new_install_prompt_erase: true,
      // Seconds to wait for Improv Serial (WiFi setup) right after flashing. The S2 has to
      // be reset by hand (native USB), so it can't answer then: wait only briefly. Not 0:
      // ESP Web Tools then never asks for Improv at all, not even when the port is opened
      // again later with the firmware running.
      new_install_improv_wait_time: board.chipFamily === 'ESP32-S2' ? 1 : 10,
      // merged image (bootloader, partitions, SafeBoot, app, file system), written at 0x0
      builds: [{ chipFamily: board.chipFamily, parts: [{ path: asset.name, offset: 0 }] }],
    }
    const manifestName = `${id}.manifest.json`
    await writeFile(join(dir, manifestName), JSON.stringify(manifest, null, 2))
    const build = { board: boardId, label: board.label, chipFamily: board.chipFamily, manifest: `${tag}/${manifestName}` }
    // app image for updates over WiFi (SafeBoot), keeps WiFi settings and material database
    const app = release.assets.find((a) => a.name === `${id}${APP_SUFFIX}`)
    if (app) {
      console.log(`${tag}: ${app.name}`)
      await download(app.browser_download_url, join(dir, app.name))
      build.app = `${tag}/${app.name}`
      build.appSize = app.size
    }
    builds.push(build)
  }
  return {
    tag,
    version,
    prerelease: release.prerelease,
    published: release.published_at,
    url: release.html_url,
    builds,
  }
}

const releases = (await api('/releases?per_page=30')).filter((r) => !r.draft)
const stable = releases.find((r) => !r.prerelease)
// releases are listed newest first: a pre-release before the stable one is newer
const beta = releases.find((r) => r.prerelease && (!stable || r.published_at > stable.published_at))

const index = { repo, releases: [] }
for (const release of [stable, beta].filter(Boolean)) index.releases.push(await publish(release))
await mkdir(outDir, { recursive: true })
await writeFile(join(outDir, 'index.json'), JSON.stringify(index, null, 2))
console.log(`Wrote ${join(outDir, 'index.json')}: ${index.releases.map((r) => r.tag).join(', ') || 'no releases'}`)
