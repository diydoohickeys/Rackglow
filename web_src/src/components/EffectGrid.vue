<script setup>
import { computed, ref, watch } from 'vue';

// The web twin of the device's Effects tab (src/ui/EffectGrid.cpp): the same
// split into patterns and audio-reactive, the same tiles, the same colour
// signatures. The colours arrive in the `animations` WebSocket message rather
// than being duplicated here, so the two pickers can't drift apart.

const props = defineProps({
  animations: { type: Array, default: () => [] },
  selected: { type: Number, default: -1 }      // -1 = no effect
});

const emit = defineEmits(['select']);

const showAudio = ref(false);

const hex = (n) => '#' + (n >>> 0).toString(16).padStart(6, '0');
// The catalog suffixes audio-reactive names with " (A)"; the section heading
// already says so.
const shortName = (name) => name.replace(/\s*\(A\)\s*$/, '');

const patterns = computed(() => props.animations.filter((a) => !a.audio));
const audio = computed(() => props.animations.filter((a) => a.audio));
const shown = computed(() => (showAudio.value ? audio.value : patterns.value));

// Follow the selection to the page it lives on, so a change pushed from the
// device is visible rather than hidden behind the other segment.
watch([() => props.animations, () => props.selected], ([list, selected]) => {
  const current = list.find((a) => a.value === selected);
  if (current) showAudio.value = !!current.audio;
}, { immediate: true });

function choose(value) {
  emit('select', value);
}
</script>

<template>
  <section class="card">
    <div class="seg" role="tablist" aria-label="Effect category">
      <button type="button" role="tab" :aria-selected="!showAudio"
              :class="{ on: !showAudio }" @click="showAudio = false">
        Patterns
      </button>
      <button type="button" role="tab" :aria-selected="showAudio"
              :class="{ on: showAudio }" @click="showAudio = true">
        Audio Reactive
      </button>
    </div>

    <div class="grid">
      <button v-for="anim in shown"
              :key="anim.value"
              type="button"
              class="tile"
              :class="{ on: selected === anim.value }"
              :aria-pressed="selected === anim.value"
              @click="choose(anim.value)">
        <span class="strip"
              :style="{ background: `linear-gradient(90deg, ${hex(anim.colour)}, ${hex(anim.colour2)})` }"
        ></span>
        <span class="nm">{{ shortName(anim.name) }}</span>
      </button>
    </div>

    <button type="button"
            class="off-bar"
            :class="{ on: selected === -1 }"
            :aria-pressed="selected === -1"
            @click="choose(-1)">
      Effects Off
    </button>
  </section>
</template>

<style scoped>
.seg {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 2px;
  padding: 2px;
  background: var(--surface-sunken);
  border: 1px solid var(--surface-border);
  border-radius: var(--radius-button, 10px);
  margin-bottom: 0.9rem;
}

.seg button {
  padding: 0.5rem 0.25rem;
  font-size: 0.78rem;
  font-weight: 600;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: var(--text-muted);
  background: transparent;
  border: 0;
  border-radius: calc(var(--radius-button, 10px) - 2px);
  cursor: pointer;
}

.seg button.on {
  background: var(--bg-lighter);
  color: var(--text-primary);
}

.grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0.5rem;
}

.tile {
  display: flex;
  flex-direction: column;
  align-items: stretch;
  gap: 0;
  min-height: 64px;
  padding: 0;
  overflow: hidden;
  background: var(--bg-lighter);
  border: 2px solid var(--surface-border);
  border-radius: var(--radius-button, 10px);
  color: var(--text-primary);
  cursor: pointer;
  transition: border-color 0.15s ease, background 0.15s ease;
}

.tile:hover { border-color: var(--border); }

/* Same convention as the panel: a thick bright border marks active. */
.tile.on {
  border-color: var(--primary);
  background: var(--surface-elevated);
}

.tile .strip { height: 6px; flex: none; }

.tile .nm {
  flex: 1;
  display: grid;
  place-items: center;
  padding: 0.45rem 0.3rem;
  font-size: 0.85rem;
  font-weight: 600;
  text-align: center;
}

.off-bar {
  width: 100%;
  margin-top: 0.5rem;
  padding: 0.55rem;
  font-size: 0.78rem;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--text-muted);
  background: var(--surface-sunken);
  border: 2px solid var(--surface-border);
  border-radius: var(--radius-button, 10px);
  cursor: pointer;
}

.off-bar.on {
  border-color: var(--primary);
  color: var(--text-primary);
}

.tile:focus-visible,
.off-bar:focus-visible,
.seg button:focus-visible {
  outline: 2px solid var(--border-focus);
  outline-offset: 2px;
}
</style>
