// Material database (material_database.json as provided by the printer)

import { officialColors } from './colors.js'

const FALLBACK = {
  Generic: [{ key: '00001', id: '00001', name: 'PLA' }],
  Creality: [{ key: '01001', id: '01001', name: 'Hyper PLA' }],
}

// category in the brand list holding the user presets of a database
export const PRESETS = 'Presets'

export class MaterialDb {
  version = undefined
  #brands = new Map(Object.entries(FALLBACK))
  // user presets: entries sharing the id of another material, listed as their own
  // category; a tag only stores the id, so choosing one writes the id of its base material
  #presets = []

  // parse and validate a database; throws on unexpected content
  load(json) {
    const list = json?.result?.list
    if (!Array.isArray(list) || list.length === 0) throw new Error('Not a Creality material database...')

    // A database copied from the printer also contains user presets sharing the id of
    // the material they are based on (e.g. "HyPLA blue@K2 Plus 0.4 nz - RW" for 01001).
    // Tags only store the id, so list each id once, preferring the material itself over
    // presets (synced by Creality Print with a "userMaterial" path, often named "...@...").
    // The other entries become presets.
    const rank = ({ base, userMaterial }) => (userMaterial ? 2 : base.name.includes('@') ? 1 : 0)
    const byId = new Map()
    const bases = []
    for (const entry of list) {
      if (!entry?.base?.id || !entry.base.name) continue
      bases.push(entry)
      const known = byId.get(entry.base.id)
      if (!known || rank(entry) < rank(known)) byId.set(entry.base.id, entry)
    }

    const brands = new Map()
    for (const { base } of byId.values()) {
      const brand = base.brand || 'Generic'
      // "Generic PLA" -> "PLA"
      const name = base.name.startsWith(`${brand} `) ? base.name.slice(brand.length + 1) : base.name
      if (!brands.has(brand)) brands.set(brand, [])
      brands.get(brand).push({ key: base.id, id: base.id, name, colors: colorsOf(base) })
    }
    if (brands.size === 0) throw new Error('Database contains no materials')

    const presets = new Map()
    // The same preset can be listed several times (synced again, or re-created in Creality
    // Print with a new "userMaterial" path), so each name is listed once.
    for (const entry of bases) {
      if (byId.get(entry.base.id) === entry) continue
      const { base } = entry
      const key = `${base.id}:${base.name}`
      if (!presets.has(key)) presets.set(key, { key, id: base.id, name: base.name, colors: colorsOf(base) })
    }

    this.#brands = brands
    this.#presets = [...presets.values()].sort((a, b) => a.name.localeCompare(b.name))
    this.version = json.result.version
    return this
  }

  // number of user presets
  get presets() {
    return this.#presets.length
  }

  // brands, followed by the presets category if the database has presets
  get brands() {
    const brands = [...this.#brands.keys()]
    return this.#presets.length ? [...brands, PRESETS] : brands
  }

  // materials of a brand (or the presets) as [{ key, id, name, colors }]; key is unique,
  // id is what gets written to the tag
  materials(brand) {
    return brand === PRESETS ? this.#presets : (this.#brands.get(brand) ?? [])
  }

  // brand and material for a selection key (a material id or a preset key)
  find(key) {
    for (const brand of this.brands) {
      const material = this.materials(brand).find((m) => m.key === key)
      if (material) return { brand, material }
    }
    return undefined
  }

  // preset colors of a selection: from the database (a preset without colors uses those
  // of its base material), else the official Creality colors
  colors(key) {
    const material = this.find(key)?.material
    if (material?.colors?.length > 1) return material.colors
    const base = material && this.find(material.id)?.material
    if (base?.colors?.length > 1) return base.colors
    return officialColors(material?.id ?? key)
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
