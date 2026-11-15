// Find a Creality printer on the local network and download its material database

import { localFetch } from './device.js'

// model codes reported by the printer's /info endpoint
export const PRINTER_MODELS = {
  F008: 'K2 Plus',
  F012: 'K2 Pro',
  F021: 'K2',
  F018: 'Hi',
}

const PROBE_TIMEOUT = 5000
const DOWNLOAD_TIMEOUT = 10000

// ask a single host whether it is a supported printer; resolves to { ip, model } or rejects
async function probe(ip, signal) {
  const response = await localFetch(`http://${ip}/info`, { signal })
  if (!response.ok) throw new Error(`HTTP ${response.status}`)
  const { model } = await response.json()
  if (!(model in PRINTER_MODELS)) throw new Error(`unsupported model ${model}`)
  return { ip, model: PRINTER_MODELS[model] }
}

// scan the /24 subnet of the given IPv4 address
export async function findPrinter(subnetIp) {
  const controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), PROBE_TIMEOUT)
  try {
    const parts = String(subnetIp ?? '').split('.')
    if (parts.length !== 4 || parts.some((p) => !/^\d{1,3}$/.test(p) || +p > 255)) {
      throw new Error('Unknown network address of the thingy')
    }
    const prefix = parts.slice(0, 3).join('.')
    const hosts = Array.from({ length: 253 }, (_, i) => `${prefix}.${i + 2}`)
    const printer = await Promise.any(hosts.map((ip) => probe(ip, controller.signal))).catch(() => {
      throw new Error('No supported printer found on the local network')
    })
    return printer
  } finally {
    clearTimeout(timeout)
    controller.abort() // cancel the remaining probes
  }
}

export async function downloadPrinterDb(ip) {
  // without a timeout, fetch() towards a switched-off printer hangs for minutes
  let response
  try {
    response = await localFetch(`http://${ip}/downloads/defData/material_database.json`, {
      signal: AbortSignal.timeout(DOWNLOAD_TIMEOUT),
    })
  } catch (error) {
    throw new Error(error.name === 'TimeoutError' ? `No answer from ${ip}` : `Download failed (${error.message})`)
  }
  if (response.status === 404) {
    // current K2 / K2 Plus firmware doesn't serve the default database anymore
    throw new Error(
      "The printer's firmware doesn't provide the material database anymore...\n\n" +
        `Copy it from the printer via SSH (e.g. scp -O root@${ip}:/mnt/UDISK/creality/userdata/box/material_database.json .) ` +
        "and use the 'Upload Database' button instead.",
    )
  }
  if (!response.ok) throw new Error(`Download failed (HTTP ${response.status})`)
  return response.text()
}
