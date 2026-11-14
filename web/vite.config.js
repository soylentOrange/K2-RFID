// SPDX-License-Identifier: GPL-3.0-or-later
// Two build targets from one source:
//   --mode device : single self-contained HTML file, embedded into the firmware
//                   (served from http://k2rfid.local/, assets come from LittleFS)
//   --mode pages  : static site for GitHub Pages (https), talks to the device over the LAN
import { defineConfig } from 'vite'
import { viteSingleFile } from 'vite-plugin-singlefile'

const PAGES_URL = 'https://soylentorange.github.io/K2-RFID/'

export default defineConfig(({ mode }) => {
  const device = mode === 'device'
  return {
    // device: assets live at the web server root; pages: relative to the repo sub-path
    base: device ? '/' : './',
    // icons, logos, manifest, default material database (shared with LittleFS)
    publicDir: '../assets/data',
    define: {
      __DEVICE_BUILD__: JSON.stringify(device),
      __PAGES_URL__: JSON.stringify(PAGES_URL),
    },
    plugins: device ? [viteSingleFile({ removeViteModuleLoader: true })] : [],
    build: {
      outDir: device ? 'dist-device' : 'dist',
      emptyOutDir: true,
      copyPublicDir: !device,
      target: 'es2022',
      reportCompressedSize: false,
    },
    server: { host: true },
  }
})
