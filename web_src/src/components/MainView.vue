<script setup>
import { wsSend } from '../api.js';
import {
  colour, white, vu,
  animationEnabled, animationValue, animations,
  brightness,
  navigate
} from '../stores.js';
import ColourWheel from './ColourWheel.vue';
import EffectGrid from './EffectGrid.vue';
import ThemeToggle from './ThemeToggle.vue';

function toggleWhite() {
  const next = !white.value;
  wsSend.white(next);
  if (next) {
    animationValue.value = -1;
    animationEnabled.value = false;
  }
}

function toggleVu() {
  wsSend.vu(!vu.value);
}

function onColourChange(hex) {
  wsSend.colour(hex);
  animationValue.value = -1;
  animationEnabled.value = false;
}

function onEffectSelect(value) {
  animationValue.value = value;
  if (value > -1) {
    animationEnabled.value = true;
    white.value = false;
    wsSend.animation(true, value);
  } else {
    animationEnabled.value = false;
    wsSend.animation(false);
  }
}

// Only send to the device on release (`change`), not during the drag (`input`).
// The native <input type="range"> fires `input` at ~60Hz which flooded the
// device with WS messages and NVS dirty-marks, crashing it on long drags.
function onBrightnessInput(e) {
  // Track the thumb locally so the UI follows the finger without lag.
  brightness.value = parseInt(e.target.value);
}

function onBrightnessChange(e) {
  wsSend.brightness(parseInt(e.target.value));
}

function goToLedConfig(e) {
  e.preventDefault();
  navigate('/led-config');
}
</script>

<template>
  <main>
    <header>
      <h1>LED Controller</h1>
      <ThemeToggle />
    </header>

    <section class="card">
      <ColourWheel id="colour-wheel" :value="colour" @change="onColourChange" />

      <div class="control full bright">
        <div class="bright-head">
          <label for="brightness">Brightness</label>
          <span class="bright-val">{{ Math.round((brightness / 255) * 100) }}%</span>
        </div>
        <input id="brightness"
               type="range" min="0" max="255"
               :value="brightness"
               @input="onBrightnessInput"
               @change="onBrightnessChange">
      </div>

      <div class="row pills">
        <button type="button" class="btn toggle pill" :class="{ active: white }" @click="toggleWhite">
          White
        </button>
        <button type="button" class="btn toggle pill" :class="{ active: vu }" @click="toggleVu">
          VU
        </button>
      </div>
    </section>

    <EffectGrid :animations="animations"
                :selected="animationEnabled ? animationValue : -1"
                @select="onEffectSelect" />

    <nav>
      <a href="/update">Update</a>
      <a href="/setup">WiFi</a>
      <a href="/led-config" @click="goToLedConfig">Settings</a>
      <a href="/logs">Logs</a>
    </nav>
  </main>
</template>

<style scoped>
main {
  width: 100%;
  max-width: 440px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 1.4rem;
}

header h1 {
  font-size: 1.4rem;
  font-weight: 700;
  letter-spacing: -0.02em;
  background: linear-gradient(135deg, var(--text-primary) 0%, var(--primary) 100%);
  -webkit-background-clip: text;
  background-clip: text;
  -webkit-text-fill-color: transparent;
}

.row {
  display: flex;
  gap: 0.85rem;
  margin-bottom: 1.25rem;
}

.control {
  display: flex;
  flex-direction: column;
  gap: 0.45rem;
}

.control.full {
  width: 100%;
  margin-bottom: 1rem;
}

.control.full:last-child {
  margin-bottom: 0;
}

.toggle {
  width: 100%;
}

/* Brightness: caption and live percentage on one line above the track, the
   same as the device's card. */
.bright { margin-top: 1.1rem; }

.bright-head {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
}

.bright-val {
  font-size: 0.95rem;
  font-weight: 700;
  color: var(--text-primary);
  font-variant-numeric: tabular-nums;
}

/* White / VU: two equal, thumb-sized pills. */
.pills { margin-bottom: 0; }

.pill {
  flex: 1;
  min-height: 48px;
  font-weight: 600;
}

nav {
  display: flex;
  justify-content: center;
  gap: 0.4rem;
  margin-top: 1.5rem;
  flex-wrap: wrap;
}

nav a {
  color: var(--text-muted);
  text-decoration: none;
  font-size: 0.82rem;
  padding: 0.5rem 0.85rem;
  border-radius: 999px;
  border: 1px solid var(--surface-border);
  background: var(--surface-sunken);
  transition: color 0.18s ease, border-color 0.18s ease, background 0.18s ease;
}

nav a:hover {
  color: var(--primary);
  border-color: var(--primary);
  background: var(--surface);
}

@media (max-width: 480px) {
  main { padding: 1rem 0.75rem; }
  .row { flex-wrap: wrap; }
}
</style>
