// Minimal stand-in for the thingy, for developing the web app without hardware.
//   npm run build:device && npm run mock   -> http://localhost:8080/
//   npm run dev                            -> http://localhost:5173/?device=localhost:8080
// "SafeBoot" (Settings) and "Update Firmware" simulate the restart into SafeBoot and the
// update; MOCK_UPDATE_VERSION sets the version reported after an update.
import { createServer } from 'node:http'
import { readFile } from 'node:fs/promises'
import { gunzipSync } from 'node:zlib'
import { extname, join } from 'node:path'
import { WebSocketServer } from 'ws'

const PORT = Number(process.env.PORT ?? 8080)
const ROOT = new URL('..', import.meta.url).pathname
const DATA = join(ROOT, '../assets/data')
const TYPES = { '.html': 'text/html', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json', '.ico': 'image/x-icon', '.webmanifest': 'application/manifest+json' }

// MOCK_DB=path serves another database, e.g. one copied from a printer
let database = await readFile(process.env.MOCK_DB ?? join(DATA, 'material_database.json'))
const config = { cloneSerial: false, beepAvailable: true, beepOnRW: false, writeTags: false, writeEmptyTags: true, PN532: true }
const printer = { host: '', customPassword: false }
let nextId = 1
// 'app', 'down' (restarting) or 'safeboot'
let mode = 'app'
let version = '1.4.0'
function restartInto(next) {
  mode = 'down'
  for (const ws of wss.clients) ws.terminate()
  setTimeout(() => (mode = next), 3000)
}

const server = createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x')
  const reply = (status, body, type = 'text/plain') => res.writeHead(status, { 'Content-Type': type }).end(body)
  if (mode === 'down') return req.socket.destroy()
  if (mode === 'safeboot') {
    // like SafeBoot: no CORS headers
    if (req.method === 'GET' && url.pathname === '/sbversion') return reply(200, 'v3.2.2-mod')
    if (req.method === 'GET' && url.pathname === '/') return reply(200, '<h1>Mock SafeBoot</h1>', 'text/html')
    if (req.method === 'POST' && url.pathname === '/') {
      let size = 0
      for await (const chunk of req) size += chunk.length
      console.log(`SafeBoot: received ${size} bytes (size=${url.searchParams.get('size')})`)
      version = process.env.MOCK_UPDATE_VERSION ?? version
      setTimeout(() => restartInto('app'), 1000)
      return reply(200, 'Update Success! Rebooting...')
    }
    return reply(404, '')
  }
  res.setHeader('Access-Control-Allow-Origin', '*')

  if (req.method === 'POST') {
    const chunks = []
    for await (const chunk of req) chunks.push(chunk)
    const body = Buffer.concat(chunks)
    if (url.pathname === '/material_database.json') {
      // multipart: find the gzip payload (magic 1f 8b)
      const start = body.indexOf(Buffer.from([0x1f, 0x8b]))
      const end = body.lastIndexOf('\r\n--')
      database = gunzipSync(body.subarray(start, end))
      broadcast({ type: 'new_db' })
      return reply(200, 'OK')
    }
    if (url.pathname === '/api/printer') {
      const form = new URLSearchParams(body.toString())
      if (form.has('host')) printer.host = form.get('host')
      if (form.has('password')) printer.customPassword = form.get('password') !== 'creality_2024'
      return reply(200, 'OK')
    }
    if (url.pathname === '/api/fetchdb') {
      if (!printer.host) return reply(400, 'No printer configured')
      // pretend to download via SSH ("fail" as host or MOCK_SSH_FAIL=1 simulates a wrong password)
      setTimeout(() => {
        const ok = printer.host !== 'fail' && !process.env.MOCK_SSH_FAIL
        broadcast({ type: 'fetch_db', result: ok, message: ok ? `Fetched material database from ${printer.host} (25041 bytes)` : 'Login to the printer failed - check the root password' })
        if (ok) broadcast({ type: 'new_db' })
      }, 1500)
      return reply(202, 'Fetching database...')
    }
    if (url.pathname === '/api/system/safeboot') setTimeout(() => restartInto('safeboot'), 1000)
    if (url.pathname.startsWith('/api/system/')) return reply(200, `Mock: ${url.pathname} done`)
    return reply(404, '')
  }
  if (url.pathname === '/') return reply(200, await readFile(join(ROOT, 'dist-device/index.html')), 'text/html')
  if (url.pathname === '/material_database.json') return reply(200, database, 'application/json')
  if (url.pathname === '/api/printer') return reply(200, JSON.stringify({ ...printer, busy: false }), 'application/json')
  if (url.pathname === '/api/system/info')
    return reply(200, JSON.stringify({ chipModel: 'ESP32-S2', chipRevision: 0, chipCores: 1, cpuFreqMHz: 240, flashSize: 4194304, psramSize: 2097152, psramFree: 1980000, heapSize: 2300000, heapFree: 2100000, heapMinFree: 1900000, heapMaxAlloc: 1900000, internalFree: 98000, internalMaxAlloc: 31744, internalMinFree: 61000, sshStackSize: 24576, sshStackReserved: true, uptime: 123, version, buildName: 'lolin_s2_mini_extled', staticStack: true }), 'application/json')
  if (url.pathname === '/boardname') return reply(200, 'lolin_s2_mini')
  if (url.pathname === '/appversion') return reply(200, `v${version}`)
  try {
    return reply(200, await readFile(join(DATA, url.pathname)), TYPES[extname(url.pathname)] ?? 'application/octet-stream')
  } catch {
    return reply(404, '')
  }
})

const wss = new WebSocketServer({ server, path: '/ws' })
const broadcast = (msg) => wss.clients.forEach((c) => c.send(JSON.stringify(msg)))

wss.on('connection', (ws) => {
  const id = nextId++
  ws.send(JSON.stringify({ type: 'initial_config', id, host: '192.168.1.42', ...config }))
  ws.on('message', (data) => {
    if (String(data) === 'ping') return ws.send('pong')
    const msg = JSON.parse(data)
    console.log('<-', msg)
    if (msg.type === 'arm_state') {
      config.writeTags = Boolean(msg.writeTags && msg.spooldata)
      config.writeEmptyTags = msg.writeEmptyTags
      broadcast({ type: 'arm_state', origin: id, writeTags: config.writeTags, writeEmptyTags: config.writeEmptyTags, spooldata: msg.spooldata })
    } else if (msg.type === 'update_config') {
      Object.assign(config, { beepOnRW: msg.beepOnRW, cloneSerial: msg.cloneSerial })
      broadcast({ type: 'update_config', origin: id, ...config })
    }
  })
})

// simulate a spool being read every 20 s
setInterval(() => broadcast({ type: 'read_spool', uid: 'a1b2c3d4', spooldata: { type: '01001', weight: 750, color: '#C12E1F', serial: '012345' } }), 20000)

server.listen(PORT, () => console.log(`mock thingy on http://localhost:${PORT}/`))
