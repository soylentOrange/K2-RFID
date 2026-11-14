// Connection to the K2-RFID thingy: HTTP requests and the websocket.
//
// On the device itself the app is served from the thingy, so everything is same-origin.
// On GitHub Pages (https) the thingy is addressed by host name / IP. Chromium browsers
// allow such requests to the local network (after a permission prompt) when the target
// is a private IP or a *.local name ("Local Network Access").

const PING_INTERVAL = 10000
const PONG_TIMEOUT = 5000
const CONNECT_TIMEOUT = 4000
const RECONNECT_DELAY = 2000

// fetch() towards the local network; tells Chromium the target is local, so an https page
// may talk to a plain http device. Ignored by other browsers.
export function localFetch(url, options = {}) {
  const isHttpsPage = window.location.protocol === 'https:'
  return fetch(url, isHttpsPage ? { targetAddressSpace: 'local', ...options } : options)
}

export class Device extends EventTarget {
  #host = ''
  #ws = null
  #pingTimer = null
  #pongTimer = null
  #connectTimer = null
  #reconnectTimer = null

  // host: '' => same origin (embedded app), otherwise "k2rfid.local" or an IP address
  get host() {
    return this.#host
  }

  get connected() {
    return this.#ws?.readyState === WebSocket.OPEN
  }

  url(path) {
    return this.#host ? `http://${this.#host}${path}` : path
  }

  fetch(path, options) {
    return localFetch(this.url(path), options)
  }

  async text(path) {
    const response = await this.fetch(path)
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    return response.text()
  }

  async post(path, body = '', contentType = 'text/plain') {
    const response = await this.fetch(path, { method: 'POST', headers: { 'Content-Type': contentType }, body })
    const text = await response.text()
    if (!response.ok) throw new Error(text || `Failed! (HTTP ${response.status})`)
    return text
  }

  send(message) {
    if (this.connected) this.#ws.send(JSON.stringify(message))
  }

  connect(host = this.#host) {
    this.#host = host
    this.disconnect()
    this.dispatchEvent(new Event('connecting'))

    const ws = new WebSocket(`ws://${host || window.location.host}/ws`)
    this.#ws = ws
    this.#connectTimer = setTimeout(() => ws.close(), CONNECT_TIMEOUT)

    ws.onopen = () => {
      clearTimeout(this.#connectTimer)
      this.#pingTimer = setInterval(() => this.#ping(), PING_INTERVAL)
      this.dispatchEvent(new Event('open'))
    }
    ws.onmessage = (event) => {
      if (event.data === 'pong') {
        clearTimeout(this.#pongTimer)
        this.#pongTimer = null
        return
      }
      let message
      try {
        message = JSON.parse(event.data)
      } catch {
        return // ignore malformed messages
      }
      this.dispatchEvent(new CustomEvent('message', { detail: message }))
    }
    ws.onclose = () => {
      if (ws !== this.#ws) return // replaced by a newer connection
      this.#stopTimers()
      this.dispatchEvent(new Event('close'))
      this.#reconnectTimer = setTimeout(() => this.connect(), RECONNECT_DELAY)
    }
  }

  disconnect() {
    this.#stopTimers()
    const ws = this.#ws
    this.#ws = null
    if (ws) {
      ws.onclose = null
      ws.close()
    }
  }

  #ping() {
    if (!this.connected || this.#pongTimer) return
    this.#pongTimer = setTimeout(() => {
      this.dispatchEvent(new Event('timeout'))
      this.#ws?.close()
    }, PONG_TIMEOUT)
    this.#ws.send('ping')
  }

  #stopTimers() {
    clearTimeout(this.#connectTimer)
    clearTimeout(this.#pongTimer)
    clearTimeout(this.#reconnectTimer)
    clearInterval(this.#pingTimer)
    this.#pongTimer = null
  }
}
