// SPDX-License-Identifier: GPL-3.0-or-later
// Every page has to fit on one screen without scrolling. Instead of fixed breakpoints
// (which don't match the real height of the content in every browser, font and page),
// measure: step by step hide less important parts and tighten the layout until the
// page fits. The steps are classes on <html>, see style.css.
const STEPS = ['hide-1', 'hide-2', 'compact', 'hide-3']

export function fit() {
  const root = document.documentElement
  root.classList.remove(...STEPS)
  for (const step of STEPS) {
    if (root.scrollHeight <= root.clientHeight) break
    root.classList.add(step)
  }
}

// refit when the window size or the content of the page changes
export function fitToScreen(content = document.body) {
  let pending = false
  const schedule = () => {
    if (pending) return
    pending = true
    requestAnimationFrame(() => {
      pending = false
      fit()
    })
  }
  window.addEventListener('resize', schedule)
  new MutationObserver(schedule).observe(content, {
    subtree: true, childList: true, characterData: true, attributes: true, attributeFilter: ['hidden'],
  })
  fit()
}
