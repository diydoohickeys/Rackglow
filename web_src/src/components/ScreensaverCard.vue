<script setup>
import { ref, shallowRef, watch } from 'vue';

// Applied live, no restart. Scene names come from the device, so this list
// can't drift out of step with what ScenePlayer actually has registered.

const props = defineProps({
  settings: { type: Object, default: null },
  loaded: { type: Boolean, default: false }
});

const enabled = ref(true);
const timeout = ref(30);
const scenes = shallowRef([]);      // replaced wholesale, never mutated
const scene = ref(0);

const status = ref('idle');         // 'idle' | 'saving' | 'saved' | 'error'
const error = ref('');

watch(() => props.settings, (data) => {
  if (!data) return;
  enabled.value = !!data.screensaverEnabled;
  if (Array.isArray(data.screensaverScenes)) scenes.value = data.screensaverScenes;
  if (typeof data.screensaverScene === 'number') scene.value = data.screensaverScene;
  if (typeof data.screensaverTimeoutSec === 'number') timeout.value = data.screensaverTimeoutSec;
}, { immediate: true });

async function save() {
  status.value = 'saving';
  error.value = '';
  try {
    const body = new URLSearchParams();
    body.set('screensaver_enabled', enabled.value ? 'true' : 'false');
    body.set('screensaver_timeout_sec', String(parseInt(timeout.value) || 30));
    body.set('screensaver_scene', String(scene.value));

    const r = await fetch('/save-settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);

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
    <h3 class="section-title">Idle Screensaver</h3>
    <p class="text-muted section-desc">
      Show the full-screen VU meter on the device when the screen has been idle.
    </p>

    <p v-if="!loaded" class="text-muted">Loading…</p>
    <template v-else>
      <div class="control row-control">
        <label for="screensaver_enabled">Enabled</label>
        <button id="screensaver_enabled"
                type="button"
                class="btn toggle"
                :class="{ active: enabled }"
                @click="enabled = !enabled">
          {{ enabled ? 'On' : 'Off' }}
        </button>
      </div>

      <template v-if="enabled">
        <div v-if="scenes.length" class="control">
          <label for="screensaver_scene">Visual</label>
          <select id="screensaver_scene" v-model="scene">
            <option v-for="(sceneName, i) in scenes" :key="i" :value="i">{{ sceneName }}</option>
          </select>
          <p class="text-muted hint">
            Swiping left or right on the screensaver also switches visual, but
            only this is remembered across a restart.
          </p>
        </div>

        <div class="control">
          <label for="screensaver_timeout">Idle Timeout (seconds)</label>
          <input id="screensaver_timeout" type="number" min="5" max="600"
                 v-model.number="timeout">
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

.hint {
  margin: 0.35rem 0 0;
  font-size: 0.78rem;
  line-height: 1.45;
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
