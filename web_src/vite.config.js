import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

// Device address for `npm run dev` — defaults to the mDNS hostname.
// Override with ESP32_HOST=192.168.x.x npm run dev if mDNS isn't resolving.
const DEVICE_HOST = process.env.ESP32_HOST || 'rackglow.local';
// ESP32_HTTPS=1 when HTTPS is turned on at the device's /setup (it serves plain HTTP otherwise);
// its certificate may be self-signed, hence secure: false.
const DEVICE_HTTPS = process.env.ESP32_HTTPS === '1';
const httpTarget = `${DEVICE_HTTPS ? 'https' : 'http'}://${DEVICE_HOST}`;
const wsTarget = `${DEVICE_HTTPS ? 'wss' : 'ws'}://${DEVICE_HOST}`;

// changeOrigin rewrites Host but not Origin, and the device refuses a POST whose Origin
// doesn't match its Host (sign-in included), so the proxy presents the device's own origin.
const toDevice = (target) => ({
  target,
  changeOrigin: true,
  secure: false,
  configure: (proxy) => {
    proxy.on('proxyReq', (req) => req.setHeader('origin', httpTarget));
  }
});

export default defineConfig({
  plugins: [vue()],
  // Compile the Options API and the devtools hooks out of the bundle: this app
  // is Composition-API-only and ships to flash, where every KB is budgeted.
  define: {
    __VUE_OPTIONS_API__: false,
    __VUE_PROD_DEVTOOLS__: false,
    __VUE_PROD_HYDRATION_MISMATCH_DETAILS__: false
  },
  server: {
    port: 5173,
    proxy: {
      '/ws': {
        target: wsTarget,
        ws: true,
        changeOrigin: true,
        secure: false
      },
      // API endpoints go to the device. /led-config itself is handled by the
      // client-side router locally (and on the device by serving index.html).
      '/screenshot.png':  toDevice(httpTarget),
      '/get-led-config':  toDevice(httpTarget),
      '/save-led-config': toDevice(httpTarget),
      '/clear-led-state': toDevice(httpTarget),
      '/get-settings':    toDevice(httpTarget),
      '/save-settings':   toDevice(httpTarget),
      '/update':          toDevice(httpTarget),
      '/ota':             toDevice(httpTarget),
      '/setup':           toDevice(httpTarget),
      '/logs':            toDevice(httpTarget),
      '/login':           toDevice(httpTarget),
      '/logout':          toDevice(httpTarget),
      '/api':             toDevice(httpTarget),
      '/wifi-setup-':     toDevice(httpTarget)
    }
  },
  build: {
    // build_web.py picks these up and embeds them in the firmware image
    // (see include/WebAssets.h) - nothing is flashed to a filesystem any more.
    outDir: 'dist',
    emptyOutDir: true,
    // Single JS/CSS bundles — no chunking. build_web.py emits one C array per
    // file and WebApi serves them by fixed name, so the names must not move.
    rollupOptions: {
      output: {
        entryFileNames: 'app.js',
        chunkFileNames: 'app.js',
        assetFileNames: (assetInfo) => {
          if (assetInfo.name.endsWith('.css')) return 'app.css';
          return assetInfo.name;
        }
      }
    }
  }
});
