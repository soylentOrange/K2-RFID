// Minimal stand-in for the thingy, for developing the web app without hardware.
//   npm run build:device && npm run mock   -> http://localhost:8080/
//   npm run dev                            -> http://localhost:5173/?device=localhost:8080
import { createServer } from 'node:http'
import { readFile } from 'node:fs/promises'
import { gunzipSync } from 'node:zlib'
import { extname, join } from 'node:path'
import { WebSocketServer } from 'ws'

const PORT = Number(process.env.PORT ?? 8080)
const ROOT = new URL('..', import.meta.url).pathname
const DATA = join(ROOT, '../assets/data')
const TYPES = { '.html': 'text/html', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json', '.ico': 'image/x-icon', '.webmanifest': 'application/manifest+json' }

let database = await readFile(join(DATA, 'material_database.json'))
const config = { cloneSerial: false, beepAvailable: true, beepOnRW: false, writeTags: false, writeEmptyTags: true, PN532: true }
let nextId = 1

const server = createServer(async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*')
  const url = new URL(req.url, 'http://x')
  const reply = (status, body, type = 'text/plain') => res.writeHead(status, { 'Content-Type': type }).end(body)

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
    if (url.pathname.startsWith('/api/system/')) return reply(200, `Mock: ${url.pathname} done`)
    return reply(404, '')
  }
  if (url.pathname === '/') return reply(200, await readFile(join(ROOT, 'dist-device/index.html')), 'text/html')
  if (url.pathname === '/material_database.json') return reply(200, database, 'application/json')
  if (url.pathname === '/boardname') return reply(200, 'mock_board')
  if (url.pathname === '/appversion') return reply(200, 'v0.0.0-mock')
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
