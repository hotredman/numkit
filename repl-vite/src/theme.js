/**
 * theme.js — Dark & Light themes for MLab IDE
 *
 * Usage:
 *   import { getTheme, applyTheme, FONT, FONT_UI } from '../theme';
 *   const C = getTheme();        // returns current palette
 *   applyTheme('light');          // switches and applies CSS vars
 */

export const FONT = "'IBM Plex Mono', 'JetBrains Mono', 'Fira Code', 'SF Mono', Consolas, monospace";
export const FONT_UI = "'DM Sans', 'IBM Plex Mono', sans-serif";

const dark = {
  name: 'dark',
  bg0: '#0a0a12',
  bg1: '#111119',
  bg2: '#18182a',
  bg3: '#222240',
  bg4: '#2c2c4e',
  border: '#2a2a48',
  borderHi: '#3e3e6e',
  text: '#d4d4f0',
  textDim: '#8888b0',
  textMuted: '#55557a',
  accent: '#7c6ff0',
  accentDim: '#5a50b0',
  green: '#6ee7a0',
  greenDim: '#3a8a5a',
  red: '#f07070',
  orange: '#f0a060',
  cyan: '#60d0f0',
  yellow: '#e8d060',
  pink: '#e070c0',

  // Syntax highlighting
  synKeyword: '#c678dd',
  synFunction: '#61afef',
  synNumber: '#d19a66',
  synString: '#98c379',
  synComment: '#5c6370',
  synOperator: '#56b6c2',
  synVariable: '#e06c75',
};

const light = {
  name: 'light',
  bg0: '#f5f5f5',
  bg1: '#ffffff',
  bg2: '#f0f0f4',
  bg3: '#e4e4ec',
  bg4: '#d8d8e4',
  border: '#d0d0dc',
  borderHi: '#b0b0c8',
  text: '#1e1e2e',
  textDim: '#555570',
  textMuted: '#8888a0',
  accent: '#6c5ce7',
  accentDim: '#8577ed',
  green: '#2d8659',
  greenDim: '#4aa872',
  red: '#d04040',
  orange: '#c07020',
  cyan: '#2090b0',
  yellow: '#a08000',
  pink: '#b050a0',

  // Syntax highlighting
  synKeyword: '#a626a4',
  synFunction: '#4078f2',
  synNumber: '#986801',
  synString: '#50a14f',
  synComment: '#a0a1a7',
  synOperator: '#0184bc',
  synVariable: '#e45649',
};

const themes = { dark, light };
let currentThemeName = 'dark';

/** Get the current theme palette */
export function getTheme() {
  return themes[currentThemeName];
}

/** Get theme by name */
export function getThemeByName(name) {
  return themes[name] || themes.dark;
}

/** Get current theme name */
export function getThemeName() {
  return currentThemeName;
}

/** Apply theme: set CSS variables on :root and update current */
export function applyTheme(name) {
  const t = themes[name];
  if (!t) return;
  currentThemeName = name;
  const root = document.documentElement;
  for (const [key, val] of Object.entries(t)) {
    if (key === 'name') continue;
    root.style.setProperty(`--c-${key}`, val);
  }
  root.style.setProperty('--font-mono', FONT);
  root.style.setProperty('--font-ui', FONT_UI);
  // Set body background for scrollbar / overflow areas
  document.body.style.background = t.bg0;
  document.body.style.color = t.text;
}

// Default export for backward compatibility: returns dark theme
// Components should import { getTheme } instead for dynamic theming
const C = dark;
export default C;
