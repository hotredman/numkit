// Splitter position (runes in a module — Svelte 5 .svelte.ts): lives
// at module level so it SURVIVES ScriptPage remounts when the user
// navigates between gallery scripts. Mutate the property, never
// reassign the export.
export const splitFrac = $state({ value: 0.45 });
