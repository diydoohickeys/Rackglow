<script setup>
// Full-page confirmation after a LED-strip save (device is restarting) or a
// state clear. Owns its own <main> layout rather than inheriting the settings
// page's, so neither view depends on the other's styles.

defineProps({
  mode: { type: String, required: true },   // 'saved' | 'cleared'
  countdown: { type: Number, default: 0 }
});

defineEmits(['back']);
</script>

<template>
  <main class="centered">
    <div v-if="mode === 'saved'" class="card status-card">
      <div class="icon">&#10003;</div>
      <h1>Configuration Saved</h1>
      <p>Your LED configuration has been saved.</p>
      <p class="status-line">Restarting device… ({{ countdown }}s)</p>
    </div>

    <div v-else class="card status-card">
      <div class="icon">&#8635;</div>
      <h1>State Cleared</h1>
      <p>Saved LED preferences have been deleted.</p>
      <p>On next boot the device will fade in to red (first-boot behaviour).</p>
      <a class="back-pill" href="/led-config" @click.prevent="$emit('back')">
        &larr; Back to Settings
      </a>
    </div>
  </main>
</template>

<style scoped>
main {
  width: 100%;
  max-width: 440px;
  margin: 0 auto;
  padding: 2rem 1rem;
}

main.centered {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
}

.status-card {
  max-width: 380px;
  width: 100%;
  text-align: center;
  padding: 2rem 1.5rem;
}

.status-card .icon {
  font-size: 3rem;
  color: var(--primary);
  margin-bottom: 1rem;
  text-shadow: 0 0 24px var(--primary-glow);
}

.status-card h1 {
  font-size: 1.2rem;
  font-weight: 700;
  margin-bottom: 0.75rem;
  background: linear-gradient(135deg, var(--text-primary) 0%, var(--primary) 100%);
  -webkit-background-clip: text;
  background-clip: text;
  -webkit-text-fill-color: transparent;
}

.status-card p {
  color: var(--text-muted);
  font-size: 0.9rem;
  margin-bottom: 0.5rem;
}

.status-line {
  margin-top: 1rem;
  font-size: 0.85rem;
}

.back-pill {
  display: inline-block;
  margin-top: 1.25rem;
  color: var(--primary);
  text-decoration: none;
  font-size: 0.9rem;
  padding: 0.5rem 1rem;
  border: 1px solid var(--surface-border);
  border-radius: 999px;
  background: var(--surface-sunken);
  transition: border-color 0.18s, color 0.18s;
}

.back-pill:hover {
  border-color: var(--primary);
}
</style>
