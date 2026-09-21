<script setup>
// Router shell only, and deliberately style-free: a scoped rule here would also
// land on the root element of whichever view is mounted (Vue applies the parent's
// scope id to a child's root), so `main { … }` in this file would reach into both
// views. Each view owns its own layout.
import { onMounted, onUnmounted } from 'vue';
import { connectWebSocket } from './api.js';
import { route, pathToRoute } from './stores.js';
import MainView from './components/MainView.vue';
import LedConfig from './components/LedConfig.vue';
import ThemeToggle from './components/ThemeToggle.vue';

function onPopState() {
  route.value = pathToRoute(window.location.pathname);
}

onMounted(() => {
  connectWebSocket();
  window.addEventListener('popstate', onPopState);
});

// Vue ignores a value returned from onMounted, so the listener comes off here.
onUnmounted(() => {
  window.removeEventListener('popstate', onPopState);
});
</script>

<template>
  <LedConfig v-if="route === 'led-config'">
    <template #theme-toggle><ThemeToggle /></template>
  </LedConfig>
  <MainView v-else />
</template>
