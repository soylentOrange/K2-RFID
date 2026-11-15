// Material database (material_database.json as provided by the printer)

import { officialColors } from './colors.js'

const FALLBACK = {
  Generic: [{ id: '00001', name: 'PLA' }],
  Creality: [{ id: '01001', name: 'Hyper PLA' }],
}

export class MaterialDb {
  version = undefined
  presets = 0 // entries sharing the id of another material (user presets)
  #brands = new Map(Object.entries(FALLBACK))

  // parse and validate a database; throws on unexpected content
  load(json) {
    const list = json?.result?.list
    if (!Array.isArray(list) || list.length === 0) throw new Error('Not a Creality material database...')

    // A database copied from the printer also contains user presets sharing the id of
    // the material they are based on (e.g. "HyPLA blue@K2 Plus 0.4 nz - RW" for 01001).
    // Tags only store the id, so list each id once, preferring the plain material name.
    const byId = new Map()
    let entries = 0
    for (const { base } of list) {
      if (!base?.id || !base?.name) continue
      entries++
      const known = byId.get(base.id)
      if (!known || (known.name.includes('@') && !base.name.includes('@'))) byId.set(base.id, base)
    }

    const brands = new Map()
    for (const base of byId.values()) {
      const brand = base.brand || 'Generic'
      // "Generic PLA" -> "PLA"
      const name = base.name.startsWith(`${brand} `) ? base.name.slice(brand.length + 1) : base.name
      if (!brands.has(brand)) brands.set(brand, [])
      brands.get(brand).push({ id: base.id, name, colors: colorsOf(base) })
    }
    if (brands.size === 0) throw new Error('Database contains no materials')

    this.#brands = brands
    this.version = json.result.version
    this.presets = entries - byId.size
    return this
  }

  get brands() {
    return [...this.#brands.keys()]
  }

  materials(brand) {
    return this.#brands.get(brand) ?? []
  }

  brandOf(id) {
    for (const [brand, materials] of this.#brands) {
      if (materials.some((m) => m.id === id)) return brand
    }
    return undefined
  }

  // preset colors of a material: from the database, else the official Creality colors
  colors(id) {
    for (const materials of this.#brands.values()) {
      const material = materials.find((m) => m.id === id)
      if (material?.colors?.length > 1) return material.colors
    }
    return officialColors(id)
  }

  get count() {
    let n = 0
    for (const materials of this.#brands.values()) n += materials.length
    return n
  }
}

// colors listed for a material as [{ hex, name }]; tags have no alpha channel, so
// "#FFFFFF00" (transparent) is reduced to "#FFFFFF" and duplicates are dropped
function colorsOf(base) {
  const names = new Map()
  for (const info of base.mixColorInfoList ?? []) {
    if (info?.hexValue) names.set(String(info.hexValue).toUpperCase(), info.colorEn?.trim() ?? '')
  }
  const colors = new Map()
  for (const value of base.colors ?? []) {
    const full = String(value).toUpperCase()
    const hex = full.slice(0, 7)
    if (!/^#[0-9A-F]{6}$/.test(hex) || colors.has(hex)) continue
    colors.set(hex, names.get(full) || names.get(hex) || '')
  }
  return [...colors].map(([hex, name]) => ({ hex, name }))
}

// gzip a string (the thingy stores the database compressed)
export function gzip(text) {
  const stream = new Blob([text]).stream().pipeThrough(new CompressionStream('gzip'))
  return new Response(stream).blob()
}
