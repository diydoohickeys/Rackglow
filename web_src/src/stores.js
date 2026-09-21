import { ref, shallowRef } from 'vue';

// LED state, mirrored from the device over WebSocket
export const colour = ref('#ff0000');
export const white = ref(false);
export const vu = ref(false);
export const animationEnabled = ref(false);
export const animationValue = ref(-1);   // -1 = no animation selected
export const brightness = ref(128);

// Animation list — populated when the device sends the "animations" message.
// shallowRef: replaced wholesale, never mutated entry by entry, so a deep proxy
// over it would be pure cost.
export const animations = shallowRef([]);   // [{ name, value }]

// Path-based view routing. Kept module-level so any component can read it.
// Values match window.location.pathname so deep links and the LVGL device's
// /led-config link both land on the right view.
export const route = ref(pathToRoute(typeof window !== 'undefined' ? window.location.pathname : '/'));

export function pathToRoute(pathname) {
  if (pathname.startsWith('/led-config')) return 'led-config';
  return 'main';
}

export function navigate(pathname) {
  if (typeof window === 'undefined') return;
  if (window.location.pathname !== pathname) {
    window.history.pushState({}, '', pathname);
  }
  route.value = pathToRoute(pathname);
}
