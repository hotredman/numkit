// Hash router (rule R5 — pure, framework-free parsing; the Svelte
// side only listens to hashchange). Routes:
//   #/                 → catalog (category cards + search)
//   #/c/<category>     → category page (script cards)
//   #/s/<cat>/<script> → script page (controls | editor | figures)

export type Route =
  | { page: 'catalog' }
  | { page: 'category'; id: string }
  | { page: 'script'; id: string };

export function parseHash(hash: string): Route {
  const h = hash.replace(/^#\/?/, '');
  if (!h) return { page: 'catalog' };
  if (h.startsWith('c/')) return { page: 'category', id: h.slice(2) };
  if (h.startsWith('s/')) return { page: 'script', id: h.slice(2) };
  return { page: 'catalog' };
}

export function hrefFor(route: Route): string {
  switch (route.page) {
    case 'catalog': return '#/';
    case 'category': return `#/c/${route.id}`;
    case 'script': return `#/s/${route.id}`;
  }
}

export function navigate(route: Route): void {
  location.hash = hrefFor(route);
}
