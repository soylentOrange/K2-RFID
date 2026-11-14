// Tiny wrapper around localStorage (JSON values, never throws)
const PREFIX = 'k2rfid.'

export function load(key, fallback) {
  try {
    const value = localStorage.getItem(PREFIX + key)
    return value === null ? fallback : JSON.parse(value)
  } catch {
    return fallback
  }
}

export function save(key, value) {
  try {
    localStorage.setItem(PREFIX + key, JSON.stringify(value))
  } catch {
    // storage unavailable (private mode, quota) - just don't remember
  }
}
