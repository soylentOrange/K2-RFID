// Inline SVG icons, usable as <img src> via data URLs
const svg = (body) =>
  `data:image/svg+xml,${encodeURIComponent(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none">${body}</svg>`)}`

export const successIcon = svg(
  '<path fill="#16c79a" d="M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16M2 12C2 6.477 6.477 2 12 2s10 4.477 10 10-4.477 10-10 10S2 17.523 2 12m14.664-3.247a1 1 0 0 1 .083 1.411l-5.333 6a1 1 0 0 1-1.495 0l-2.666-3a1 1 0 0 1 1.494-1.328l1.92 2.159 4.586-5.16a1 1 0 0 1 1.411-.082"/>',
)

export const errorIcon = svg(
  '<path fill="#ec4646" d="M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16M2 12C2 6.477 6.477 2 12 2s10 4.477 10 10-4.477 10-10 10S2 17.523 2 12m5.793-4.207a1 1 0 0 1 1.414 0L12 10.586l2.793-2.793a1 1 0 1 1 1.414 1.414L13.414 12l2.793 2.793a1 1 0 0 1-1.414 1.414L12 13.414l-2.793 2.793a1 1 0 0 1-1.414-1.414L10.586 12 7.793 9.207a1 1 0 0 1 0-1.414"/>',
)

export const tagIcon = svg(
  '<path stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 5h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-3m-8 0a5 5 0 0 0-5-5m9 5a9 9 0 0 0-9-9m0 8.5c0-.276.237-.51.48-.378.167.092.306.23.398.398.132.243-.102.48-.378.48a.5.5 0 0 1-.5-.5"/>',
)

export const spoolIcon = (color) =>
  svg(`<circle cx="12" cy="12" r="9" fill="${color}" stroke="#0d0d0d" stroke-width="2"/>`)
