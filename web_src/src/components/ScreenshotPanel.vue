<script setup>
import { computed, onUnmounted, ref } from 'vue';

// Grabs GET /screenshot.png off the device. Fetched into a blob rather than
// pointed at with <img src>, because the device serves one screenshot at a
// time (409 otherwise) — a blob means the preview and the Download button are
// the same single request, and saving is instant.

const url = ref(null);          // object URL of the last capture
const takenAt = ref(null);
const bytes = ref(0);
const busy = ref(false);
const error = ref('');

function release() {
  if (url.value) {
    URL.revokeObjectURL(url.value);
    url.value = null;
  }
}

async function capture() {
  busy.value = true;
  error.value = '';
  try {
    // The device sends no-store, but a proxy in between might not.
    const res = await fetch(`/screenshot.png?t=${Date.now()}`);
    if (!res.ok) {
      error.value = res.status === 409
        ? 'Another screenshot is still being sent — try again in a moment.'
        : res.status === 503
          ? 'The device is out of memory for the screenshot encoder.'
          : `Screenshot failed (HTTP ${res.status}).`;
      return;
    }
    const blob = await res.blob();
    release();
    url.value = URL.createObjectURL(blob);
    bytes.value = blob.size;
    takenAt.value = new Date();
  } catch {
    error.value = 'Could not reach the device.';
  } finally {
    busy.value = false;
  }
}

onUnmounted(release);

// Colon-free, so it is a valid filename on every OS.
const filename = computed(() => (takenAt.value
  ? `screen-${takenAt.value.toISOString().slice(0, 19).replace(/[:T]/g, '-')}.png`
  : 'screen.png'));
</script>

<template>
  <section class="card">
    <h3 class="section-title">Screenshot</h3>
    <p class="text-muted section-desc">
      Capture what's on the device screen right now as a PNG — for help docs and bug reports.
    </p>

    <div class="shot-actions">
      <button type="button" class="btn btn-primary" :disabled="busy" @click="capture">
        {{ busy ? 'Capturing…' : url ? 'Retake' : 'Take screenshot' }}
      </button>
      <a v-if="url" class="btn" :href="url" :download="filename">Download</a>
    </div>

    <p v-if="error" class="shot-error">{{ error }}</p>

    <figure v-if="url" class="shot">
      <img :src="url" alt="The device screen at the moment of capture" />
      <figcaption class="text-muted">
        {{ takenAt.toLocaleTimeString() }} · {{ Math.round(bytes / 1024) }} KB
      </figcaption>
    </figure>
  </section>
</template>

<style scoped>
.shot-actions {
  display: flex;
  gap: 0.6rem;
  flex-wrap: wrap;
  align-items: center;
}

/* The <a> is a button by role here, so it needs the line-height a <button>
   gets for free. */
.shot-actions a.btn {
  display: inline-flex;
  align-items: center;
  text-decoration: none;
}

.shot-error {
  margin: 0.75rem 0 0;
  color: var(--danger);
  font-size: 0.85rem;
}

.shot {
  margin: 1rem 0 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5rem;
}

/* The panel is a tall 320x480, so cap the height rather than the width or it
   runs off the bottom of a phone. */
.shot img {
  max-width: 100%;
  max-height: 60vh;
  border: 1px solid var(--border);
  border-radius: var(--radius-card);
  background: var(--bg-dark);
}

.shot figcaption {
  font-size: 0.8rem;
}
</style>
