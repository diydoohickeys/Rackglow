<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue';
import { navigate } from '../stores.js';
import ConfigStatusCard from './ConfigStatusCard.vue';
import MqttCard from './MqttCard.vue';
import ScreensaverCard from './ScreensaverCard.vue';
import ScreenshotPanel from './ScreenshotPanel.vue';

const numStrips = ref('');
const ledsPerStrip = ref('');
const loaded = ref(false);

// 'idle' | 'saving' | 'saved' | 'clearing' | 'cleared' | 'error'
const status = ref('idle');
const errorMessage = ref('');
const restartCountdown = ref(0);
let restartTick = null;

// The two live-applied cards below are driven from one /get-settings response
// rather than fetching their own — the device's httpd is single-task.
const settings = ref(null);
const settingsLoaded = ref(false);

const total = computed(() => (parseInt(numStrips.value) || 0) * (parseInt(ledsPerStrip.value) || 0));

// Awaited in sequence, not in parallel: two concurrent requests queue against
// the single-task httpd anyway, and the first one to land blocks the other.
onMounted(async () => {
  try {
    const r = await fetch('/get-led-config');
    const data = await r.json();
    if (data.hasLedSettings) {
      numStrips.value = data.numStrips || '';
      ledsPerStrip.value = data.ledsPerStrip || '';
    }
  } catch (e) {
    console.error('Failed to load LED config:', e);
  } finally {
    loaded.value = true;
  }

  try {
    const r = await fetch('/get-settings');
    settings.value = await r.json();
  } catch (e) {
    console.error('Failed to load settings:', e);
  } finally {
    settingsLoaded.value = true;
  }
});

// Vue ignores a cleanup returned from onMounted, so the restart countdown is
// cancelled here — otherwise navigating away mid-countdown leaves an interval
// that later yanks the page back to /.
onUnmounted(() => {
  if (restartTick) clearInterval(restartTick);
});

async function save() {
  if (!numStrips.value || !ledsPerStrip.value) return;
  status.value = 'saving';
  errorMessage.value = '';

  try {
    const body = new URLSearchParams();
    body.set('num_strips', String(numStrips.value));
    body.set('leds_per_strip', String(ledsPerStrip.value));

    const r = await fetch('/save-led-config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);

    status.value = 'saved';
    // The device reboots ~2s after the POST. Count down then hop back to /.
    restartCountdown.value = 5;
    restartTick = setInterval(() => {
      restartCountdown.value -= 1;
      if (restartCountdown.value <= 0) {
        clearInterval(restartTick);
        restartTick = null;
        navigate('/');
      }
    }, 1000);
  } catch (e) {
    status.value = 'error';
    errorMessage.value = String(e.message || e);
  }
}

async function clearState() {
  if (!confirm('Clear all saved LED state preferences?\n\nThis will reset brightness, color, animation, and mode settings. The device will show the default red fade-in on next boot.')) return;
  status.value = 'clearing';
  errorMessage.value = '';

  try {
    const r = await fetch('/clear-led-state', { method: 'POST' });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    status.value = 'cleared';
  } catch (e) {
    status.value = 'error';
    errorMessage.value = String(e.message || e);
  }
}

function backToMain() {
  navigate('/');
}
</script>

<template>
  <ConfigStatusCard v-if="status === 'saved' || status === 'cleared'"
                    :mode="status"
                    :countdown="restartCountdown"
                    @back="status = 'idle'" />

  <main v-else>
    <header>
      <h1>Settings</h1>
      <slot name="theme-toggle" />
    </header>

    <ScreensaverCard :settings="settings" :loaded="settingsLoaded" />
    <MqttCard :settings="settings" :loaded="settingsLoaded" />
    <ScreenshotPanel />

    <section class="card">
      <h3 class="section-title">LED Strips</h3>
      <p v-if="!loaded" class="text-muted">Loading…</p>
      <template v-else>
        <div class="control">
          <label for="num_strips">Number of Strips</label>
          <input id="num_strips" type="number" min="1" max="20" v-model.number="numStrips" required>
        </div>

        <div class="control">
          <label for="leds_per_strip">LEDs per Strip</label>
          <input id="leds_per_strip" type="number" min="1" max="300" v-model.number="ledsPerStrip" required>
        </div>

        <p class="text-muted total">Total LEDs: <strong>{{ total }}</strong></p>

        <button type="button"
                class="btn btn-primary"
                :disabled="status === 'saving' || !numStrips || !ledsPerStrip"
                @click="save">
          {{ status === 'saving' ? 'Saving…' : 'Save & Restart' }}
        </button>

        <p v-if="status === 'error'" class="error">Error: {{ errorMessage }}</p>

        <hr>

        <div class="danger-zone">
          <h3>Reset LED State</h3>
          <p class="text-muted">
            Clear saved brightness, colour, animation, and mode settings.<br>
            Device will boot with default red fade-in after restart.
          </p>
          <button type="button"
                  class="btn btn-danger"
                  :disabled="status === 'clearing'"
                  @click="clearState">
            {{ status === 'clearing' ? 'Clearing…' : 'Clear Saved State' }}
          </button>
        </div>

        <a class="back" href="/" @click.prevent="backToMain">&larr; Back</a>
      </template>
    </section>
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

.control {
  display: flex;
  flex-direction: column;
  gap: 0.45rem;
  margin-bottom: 1rem;
}

.section-title {
  font-size: 1rem;
  font-weight: 600;
  margin-bottom: 0.35rem;
}

.total {
  margin-bottom: 1rem;
}

.total strong {
  color: var(--text-primary);
  font-weight: 600;
}

.btn {
  width: 100%;
}

.btn-primary {
  margin-top: 0.5rem;
}

.btn-primary:disabled,
.btn-danger:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.error {
  color: var(--danger);
  margin-top: 0.75rem;
  font-size: 0.85rem;
}

hr {
  border: none;
  border-top: 1px solid var(--surface-border);
  margin: 1.75rem 0;
}

.danger-zone {
  text-align: center;
}

.danger-zone h3 {
  color: var(--danger);
  font-size: 1rem;
  font-weight: 600;
  margin-bottom: 0.5rem;
}

.danger-zone p {
  margin-bottom: 1rem;
}

.back {
  display: block;
  text-align: center;
  margin-top: 1rem;
  color: var(--primary);
  text-decoration: none;
  font-size: 0.85rem;
}

.back:hover {
  text-decoration: underline;
}
</style>
