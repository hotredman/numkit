// Gallery model (rule R5 — pure): category grouping, search, lookup.
// Entry ids are path-with-slashes ("hydroacoustics/sonar-range.m");
// the first segment IS the category (mirrors play/gallery layout).

export interface GalleryEntry {
  id: string;
  title: string;
  description: string;
  source: string;
}

export interface CategoryInfo {
  id: string;
  name: string;
  count: number;
}

const CATEGORY_NAMES: Record<string, string> = {
  arrays: 'Массивы',
  hydroacoustics: 'Гидроакустика',
  rays: 'Лучи и распространение',
  comms: 'Связь',
  dsp: 'Цифровая обработка',
  control: 'Управление',
  physics: 'Физика',
};

export function categoryName(id: string): string {
  return CATEGORY_NAMES[id]
    ?? id.charAt(0).toUpperCase() + id.slice(1);
}

export function categoryOfEntry(id: string): string {
  const slash = id.indexOf('/');
  return slash < 0 ? id : id.slice(0, slash);
}

export function categoriesOf(gallery: GalleryEntry[]): CategoryInfo[] {
  const counts = new Map<string, number>();
  for (const e of gallery) {
    const cat = categoryOfEntry(e.id);
    counts.set(cat, (counts.get(cat) ?? 0) + 1);
  }
  return [...counts.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([id, count]) => ({ id, name: categoryName(id), count }));
}

export function scriptsOf(gallery: GalleryEntry[], category: string):
  GalleryEntry[] {
  const prefix = category + '/';
  return gallery.filter((e) => e.id.startsWith(prefix));
}

export function searchScripts(gallery: GalleryEntry[], query: string):
  GalleryEntry[] {
  const q = query.trim().toLowerCase();
  if (!q) return [];
  return gallery.filter((e) =>
    e.title.toLowerCase().includes(q)
    || e.description.toLowerCase().includes(q)
    || e.id.toLowerCase().includes(q));
}

export function entryById(gallery: GalleryEntry[], id: string):
  GalleryEntry | undefined {
  return gallery.find((e) => e.id === id);
}
