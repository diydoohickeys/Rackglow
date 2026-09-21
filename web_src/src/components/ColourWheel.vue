<script setup>
import { computed, ref, watch } from 'vue';

// The same HSV disc the device paints: angle = hue, distance from centre =
// saturation, value pinned at full (brightness is the fader's job). Mirroring
// the panel means a colour picked here lands the knob in the same place there.
//
// The gradient starts at 3 o'clock and runs clockwise because that is what
// atan2(dy, dx) does with y pointing down — see ColourWheel.cpp.

const props = defineProps({
  value: { type: String, default: '#ff0000' }
});

const emit = defineEmits(['change']);

const el = ref(null);
const dragging = ref(false);
const hue = ref(0);
const sat = ref(100);

function hexToHs(hex) {
  const m = /^#?([0-9a-f]{6})$/i.exec(hex || '');
  if (!m) return { hue: 0, sat: 0 };
  const n = parseInt(m[1], 16);
  const r = ((n >> 16) & 255) / 255, g = ((n >> 8) & 255) / 255, b = (n & 255) / 255;
  const max = Math.max(r, g, b), min = Math.min(r, g, b), d = max - min;
  let h = 0;
  if (d !== 0) {
    if (max === r) h = ((g - b) / d) % 6;
    else if (max === g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h *= 60;
    if (h < 0) h += 360;
  }
  return { hue: h, sat: max === 0 ? 0 : (d / max) * 100 };
}

function hsToHex(h, s) {
  const c = s / 100, x = c * (1 - Math.abs(((h / 60) % 2) - 1)), m = 1 - c;
  let rgb;
  if (h < 60) rgb = [c, x, 0];
  else if (h < 120) rgb = [x, c, 0];
  else if (h < 180) rgb = [0, c, x];
  else if (h < 240) rgb = [0, x, c];
  else if (h < 300) rgb = [x, 0, c];
  else rgb = [c, 0, x];
  return '#' + rgb
    .map((v) => Math.round((v + m) * 255).toString(16).padStart(2, '0'))
    .join('');
}

// Follow an external change (device, MQTT) unless the finger is down, so a
// state echo can't fight the drag. The dependency is listed explicitly — a
// watchEffect would also track whatever the helpers read.
watch(() => props.value, (next) => {
  if (dragging.value) return;
  const hs = hexToHs(next);
  hue.value = hs.hue;
  sat.value = hs.sat;
}, { immediate: true });

function pick(event) {
  if (!el.value) return;
  const r = el.value.getBoundingClientRect();
  const radius = r.width / 2;
  const dx = event.clientX - (r.left + radius);
  const dy = event.clientY - (r.top + radius);

  let angle = (Math.atan2(dy, dx) * 180) / Math.PI;
  if (angle < 0) angle += 360;

  // Outside the disc still picks, clamped to the rim, so a drag that
  // overshoots keeps tracking hue instead of freezing.
  hue.value = angle;
  sat.value = Math.min(100, (Math.hypot(dx, dy) / radius) * 100);

  emit('change', hsToHex(hue.value, sat.value));
}

function onPointerDown(event) {
  dragging.value = true;
  el.value.setPointerCapture(event.pointerId);
  pick(event);
}

function onPointerMove(event) {
  if (dragging.value) pick(event);
}

function onPointerUp(event) {
  if (!dragging.value) return;
  dragging.value = false;
  el.value.releasePointerCapture(event.pointerId);
}

// Arrow keys step the hue, so the wheel is operable without a pointer.
function onKeyDown(event) {
  const step = event.shiftKey ? 15 : 5;
  if (event.key === 'ArrowRight' || event.key === 'ArrowUp') hue.value = (hue.value + step) % 360;
  else if (event.key === 'ArrowLeft' || event.key === 'ArrowDown') hue.value = (hue.value + 360 - step) % 360;
  else return;
  event.preventDefault();
  if (sat.value === 0) sat.value = 100;
  emit('change', hsToHex(hue.value, sat.value));
}

const knobX = computed(() => 50 + Math.cos((hue.value * Math.PI) / 180) * (sat.value / 2));
const knobY = computed(() => 50 + Math.sin((hue.value * Math.PI) / 180) * (sat.value / 2));
</script>

<template>
  <div
    ref="el"
    class="wheel"
    :class="{ dragging }"
    role="slider"
    aria-label="Colour"
    :aria-valuetext="value"
    :aria-valuenow="Math.round(hue)"
    aria-valuemin="0"
    aria-valuemax="360"
    tabindex="0"
    @pointerdown="onPointerDown"
    @pointermove="onPointerMove"
    @pointerup="onPointerUp"
    @pointercancel="onPointerUp"
    @keydown="onKeyDown"
  >
    <span class="knob" :style="{ left: knobX + '%', top: knobY + '%', background: value }"></span>
  </div>
</template>

<style scoped>
.wheel {
  position: relative;
  width: 100%;
  max-width: 240px;
  aspect-ratio: 1;
  margin: 0 auto;
  border-radius: 50%;
  touch-action: none;
  cursor: crosshair;
  background:
    radial-gradient(circle at 50% 50%, #fff 0%, rgba(255, 255, 255, 0) 100%),
    conic-gradient(from 90deg, #f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00);
}

.wheel:focus-visible {
  outline: 2px solid var(--border-focus);
  outline-offset: 4px;
}

.knob {
  position: absolute;
  width: 26px;
  height: 26px;
  margin: -13px 0 0 -13px;
  border-radius: 50%;
  border: 3px solid #fff;
  box-shadow: 0 0 0 1px rgba(0, 0, 0, 0.45);
  pointer-events: none;
}

.wheel.dragging .knob {
  transform: scale(1.12);
}

@media (prefers-reduced-motion: reduce) {
  .wheel.dragging .knob { transform: none; }
}
</style>
