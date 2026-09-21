<script setup>
import { ref, watch } from 'vue';

// Home Assistant / MQTT broker settings, applied live, no restart.

const props = defineProps({
  settings: { type: Object, default: null },
  loaded: { type: Boolean, default: false }
});

const enabled = ref(false);
const host = ref('homeassistant.local');
const port = ref(1883);
const user = ref('');
const pass = ref('');          // write-only: blank = keep existing on save
const hasPass = ref(false);    // whether the device already has a stored password
const prefix = ref('rackglow');

const status = ref('idle');    // 'idle' | 'saving' | 'saved' | 'error'
const error = ref('');

watch(() => props.settings, (data) => {
  if (!data) return;
  enabled.value = !!data.mqttEnabled;
  if (typeof data.mqttHost === 'string') host.value = data.mqttHost;
  if (typeof data.mqttPort === 'number') port.value = data.mqttPort;
  if (typeof data.mqttUser === 'string') user.value = data.mqttUser;
  hasPass.value = !!data.mqttHasPass;
  if (typeof data.mqttPrefix === 'string') prefix.value = data.mqttPrefix;
}, { immediate: true });

async function save() {
  status.value = 'saving';
  error.value = '';
  try {
    const body = new URLSearchParams();
    body.set('mqtt_enabled', enabled.value ? 'true' : 'false');
    body.set('mqtt_host', host.value || '');
    body.set('mqtt_port', String(parseInt(port.value) || 1883));
    body.set('mqtt_user', user.value || '');
    body.set('mqtt_pass', pass.value || '');
    body.set('mqtt_prefix', prefix.value || 'rackglow');

    const r = await fetch('/save-settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);

    // Password is write-only — if one was just entered, it's now stored; clear the
    // field so it isn't resubmitted and reflect that a password exists.
    if (pass.value.length > 0) {
      hasPass.value = true;
      pass.value = '';
    }

    status.value = 'saved';
    setTimeout(() => { if (status.value === 'saved') status.value = 'idle'; }, 2000);
  } catch (e) {
    status.value = 'error';
    error.value = String(e.message || e);
  }
}
</script>

<template>
  <section class="card">
    <h3 class="section-title">Home Assistant (MQTT)</h3>
    <p class="text-muted section-desc">
      Expose the LEDs to Home Assistant via MQTT Discovery — no add-on or custom
      integration needed. Point this at your broker; entities appear automatically.
    </p>

    <p v-if="!loaded" class="text-muted">Loading…</p>
    <template v-else>
      <div class="control row-control">
        <label for="mqtt_enabled">Enabled</label>
        <button id="mqtt_enabled"
                type="button"
                class="btn toggle"
                :class="{ active: enabled }"
                @click="enabled = !enabled">
          {{ enabled ? 'On' : 'Off' }}
        </button>
      </div>

      <template v-if="enabled">
        <div class="control">
          <label for="mqtt_host">Broker Host</label>
          <input id="mqtt_host" type="text" v-model="host"
                 placeholder="homeassistant.local" autocomplete="off">
        </div>
        <div class="control">
          <label for="mqtt_port">Port</label>
          <input id="mqtt_port" type="number" min="1" max="65535" v-model.number="port">
        </div>
        <div class="control">
          <label for="mqtt_user">Username</label>
          <input id="mqtt_user" type="text" v-model="user" autocomplete="off">
        </div>
        <div class="control">
          <label for="mqtt_pass">Password</label>
          <input id="mqtt_pass" type="password" v-model="pass"
                 autocomplete="new-password"
                 :placeholder="hasPass ? '•••••••• (leave blank to keep)' : 'Set a password'">
        </div>
        <div class="control">
          <label for="mqtt_prefix">Topic Prefix</label>
          <input id="mqtt_prefix" type="text" v-model="prefix"
                 placeholder="rackglow" autocomplete="off">
        </div>
      </template>

      <button type="button"
              class="btn btn-primary"
              :disabled="status === 'saving'"
              @click="save">
        {{ status === 'saving' ? 'Saving…' : 'Save' }}
      </button>

      <p v-if="status === 'saved'" class="ok">Saved &#10003;</p>
      <p v-else-if="status === 'error'" class="error">Error: {{ error }}</p>
    </template>
  </section>
</template>

<style scoped>
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

.section-desc {
  font-size: 0.85rem;
  margin-bottom: 1.1rem;
}

/* Label beside its control (used for the on/off toggle row). */
.row-control {
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
}

.row-control label {
  margin-bottom: 0;
}

.btn {
  width: 100%;
}

/* Override the width above so the toggle stays compact beside its label. */
.btn.toggle {
  width: auto;
  min-width: 5.5rem;
}

.btn-primary {
  margin-top: 0.5rem;
}

.btn-primary:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.ok {
  color: var(--success);
  margin-top: 0.75rem;
  font-size: 0.85rem;
}

.error {
  color: var(--danger);
  margin-top: 0.75rem;
  font-size: 0.85rem;
}
</style>
