// fetch() for device routes: a 401 means the session is gone, so go and sign in, then come back.
export function redirectToSignIn() {
  const next = window.location.pathname + window.location.search;
  window.location.href = `/login?next=${encodeURIComponent(next)}`;
}

export async function deviceFetch(url, options) {
  const res = await fetch(url, options);
  if (res.status === 401) redirectToSignIn();
  return res;
}
