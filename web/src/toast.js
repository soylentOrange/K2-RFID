// Minimal toast notifications (replaces Toastify)
const container = document.getElementById('toasts')

export function toast(text, { icon, duration = 3000, onClick } = {}) {
  const el = document.createElement('div')
  el.className = 'toast'
  if (icon) {
    const img = document.createElement('img')
    img.src = icon
    img.alt = ''
    el.append(img)
  }
  const span = document.createElement('span')
  span.textContent = text
  el.append(span)

  const remove = () => {
    el.classList.add('leaving')
    setTimeout(() => el.remove(), 300)
  }
  if (onClick) {
    el.classList.add('clickable')
    el.addEventListener('click', () => {
      onClick()
      remove()
    })
  }
  container.append(el)
  setTimeout(remove, duration)
}
