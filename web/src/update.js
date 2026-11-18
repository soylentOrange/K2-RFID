// SPDX-License-Identifier: GPL-3.0-or-later
// Firmware update over WiFi, driven by the browser:
//   1. download the app image (<board>.firmware.bin) of a release from the GitHub Pages site
//      (GitHub release downloads can't be fetched cross-origin, so fetch-firmware.mjs copies them there)
//   2. restart the thingy into SafeBoot
//   3. upload the image to SafeBoot, which flashes it and restarts into the new firmware
// The browser does the https part, so neither the firmware nor SafeBoot need TLS. WiFi settings
// and the material database are kept (only the app partition is written).

// PlatformIO environment (or board name of older firmware) -> board name of the release assets
const BOARDS = {
  lolin_s2_mini: 'lolin_s2_mini',
  lolin_s2_mini_extled: 'lolin_s2_mini_external_WS2812b',
  esp32_s3_tiny: 'esp32_s3_tiny',
  waveshare_esp32_s3_tiny: 'esp32_s3_tiny',
}

// chip id in the header of an ESP32 app image (byte 12)
const CHIP_IDS = { 'ESP32-S2': 2, 'ESP32-S3': 9 }

const POLL_INTERVAL = 1000
const POLL_TIMEOUT = 3000

// base URL of the firmware on GitHub Pages (relative when the app itself runs there)
export const FIRMWARE_BASE = `${__DEVICE_BUILD__ ? __PAGES_URL__ : './'}firmware/`

// releases offered for updates (newest release and a newer pre-release, see fetch-firmware.mjs)
export async function loadReleases() {
  const response = await fetch(`${FIRMWARE_BASE}index.json`, { cache: 'no-store' })
  if (!response.ok) throw new Error(`HTTP ${response.status}`)
  return (await response.json()).releases.filter((r) => r.builds.some((b) => b.app))
}

// board of the release assets matching the running firmware (undefined if unknown)
export function boardOf(info, boardName) {
  return BOARDS[info?.buildName?.replace(/-ota$/, '')] ?? BOARDS[boardName]
}

// "1.4.1-beta" / "v1.4.1-beta" -> [1, 4, 1, prerelease]; undefined for development builds
function parseVersion(version) {
  const match = /^v?(\d+)\.(\d+)\.(\d+)(?:[-_](.+))?$/.exec(version ?? '')
  return match && { numbers: match.slice(1, 4).map(Number), prerelease: match[4] }
}

// > 0 if a is newer than b, undefined if one of them isn't a release version
export function compareVersions(a, b) {
  const va = parseVersion(a)
  const vb = parseVersion(b)
  if (!va || !vb) return undefined
  for (let i = 0; i < 3; i++) if (va.numbers[i] !== vb.numbers[i]) return va.numbers[i] - vb.numbers[i]
  if (!va.prerelease !== !vb.prerelease) return va.prerelease ? -1 : 1 // 1.4.0-beta < 1.4.0
  return (va.prerelease ?? '').localeCompare(vb.prerelease ?? '')
}

export function isPrerelease(version) {
  return Boolean(parseVersion(version)?.prerelease)
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms))

// polls check() until it returns something truthy (exceptions count as "not yet")
async function waitFor(check, timeout, message) {
  const end = Date.now() + timeout
  while (Date.now() < end) {
    try {
      const result = await check()
      if (result) return result
    } catch {
      // not reachable (yet)
    }
    await sleep(POLL_INTERVAL)
  }
  throw new Error(message)
}

// downloads and checks the app image of a build
async function download(build) {
  const response = await fetch(`${FIRMWARE_BASE}${build.app}`, { cache: 'no-store' })
  if (!response.ok) throw new Error(`Download of the firmware failed (HTTP ${response.status})`)
  const image = await response.blob()
  const header = new Uint8Array(await image.slice(0, 16).arrayBuffer())
  const chipId = CHIP_IDS[build.chipFamily]
  if (header[0] !== 0xe9 || (chipId !== undefined && header[12] !== chipId)) {
    throw new Error(`The downloaded file is no firmware for the ${build.chipFamily}`)
  }
  if (build.appSize && image.size !== build.appSize) throw new Error('The download of the firmware is incomplete')
  return image
}

// runs the whole update, progress(text) reports the steps; resolves with the system info of
// the new firmware. device: the Device of main.js (same origin or a host on the LAN)
export async function updateFirmware(device, release, build, progress) {
  progress(`Downloading ${release.tag}...`)
  const image = await download(build)
  const sameOrigin = !device.host

  progress('Restarting into SafeBoot...')
  await device.post('/api/system/safeboot')
  device.disconnect()

  // the firmware goes down first, then SafeBoot comes up (it doesn't send CORS headers, so from
  // GitHub Pages it's only possible to see that something answers)
  const poll = (path, options = {}) =>
    device.fetch(path, { cache: 'no-store', signal: AbortSignal.timeout(POLL_TIMEOUT), ...options })
  await waitFor(() => poll('/api/system/info').then(() => false, () => true), 20000, 'The thingy did not restart into SafeBoot')
  await waitFor(async () => {
    const response = await poll('/sbversion', { mode: sameOrigin ? 'same-origin' : 'no-cors' })
    return !sameOrigin || response.ok
  }, 60000, 'SafeBoot is not reachable. Open the thingy in the browser and upload the firmware there.')

  progress(`Installing ${release.tag}...\n\nDon't switch off the thingy.`)
  const form = new FormData()
  form.append('file', image, build.app.split('/').pop())
  const response = await device.fetch(`/?size=${image.size}&mode=0`, {
    method: 'POST',
    body: form,
    mode: sameOrigin ? 'same-origin' : 'no-cors',
  })
  if (sameOrigin && !response.ok) throw new Error((await response.text()) || `Update failed (HTTP ${response.status})`)

  progress('Restarting the new firmware...')
  const info = await waitFor(async () => {
    const r = await poll('/api/system/info')
    return r.ok && r.json()
  }, 90000, 'The thingy did not come back after the update')
  if (info.version && compareVersions(info.version, release.version) !== 0) {
    throw new Error(`The thingy still runs ${info.version} instead of ${release.version}`)
  }
  return info
}
