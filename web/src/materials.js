// Material database (material_database.json as provided by the printer)

const FALLBACK = {
  Generic: [{ id: '00001', name: 'PLA' }],
  Creality: [{ id: '01001', name: 'Hyper PLA' }],
}

export class MaterialDb {
  version = undefined
  #brands = new Map(Object.entries(FALLBACK))

  // parse and validate a database; throws on unexpected content
  load(json) {
    const list = json?.result?.list
    if (!Array.isArray(list) || list.length === 0) throw new Error('Not a Creality material database...')

    // A database copied from the printer also contains user presets sharing the id of
    // the material they are based on (e.g. "HyPLA blue@K2 Plus 0.4 nz - RW" for 01001).
    // Tags only store the id, so list each id once, preferring the plain material name.
    const byId = new Map()
    for (const { base } of list) {
      if (!base?.id || !base?.name) continue
      const known = byId.get(base.id)
      if (!known || (known.name.includes('@') && !base.name.includes('@'))) byId.set(base.id, base)
    }

    const brands = new Map()
    for (const base of byId.values()) {
      const brand = base.brand || 'Generic'
      // "Generic PLA" -> "PLA"
      const name = base.name.startsWith(`${brand} `) ? base.name.slice(brand.length + 1) : base.name
      if (!brands.has(brand)) brands.set(brand, [])
      brands.get(brand).push({ id: base.id, name })
    }
    if (brands.size === 0) throw new Error('Database contains no materials')

    this.#brands = brands
    this.version = json.result.version
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

  get count() {
    let n = 0
    for (const materials of this.#brands.values()) n += materials.length
    return n
  }
}

// gzip a string (the thingy stores the database compressed)
export function gzip(text) {
  const stream = new Blob([text]).stream().pipeThrough(new CompressionStream('gzip'))
  return new Response(stream).blob()
}
