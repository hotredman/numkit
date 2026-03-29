/**
 * theme.js — Dark & Light themes for MLab IDE
 *
 * Syntax token types:
 *   synKeyword   — control flow: for, if, while, end, function, return...
 *   synBuiltin   — built-in functions: plot, sin, zeros, disp...
 *   synNumber    — numeric literals: 42, 3.14, 1e-3, 2i
 *   synString    — string literals: 'hello'
 *   synComment   — comments: % this is a comment
 *   synOperator  — operators: + - * / = == ~ & |
 *   synConstant  — language constants: pi, eps, inf, nan, true, false
 *   synParam     — command parameters: on, off, all, minor, equal, tight...
 */

export const FONT = "'IBM Plex Mono', 'JetBrains Mono', 'Fira Code', 'SF Mono', Consolas, monospace";
export const FONT_UI = "'DM Sans', 'IBM Plex Mono', sans-serif";

const dark = {
  name: 'dark',

  // UI surfaces
  bg0: '#0a0a12',
  bg1: '#111119',
  bg2: '#18182a',
  bg3: '#222240',
  bg4: '#2c2c4e',
  border: '#2a2a48',
  borderHi: '#3e3e6e',

  // UI text
  text: '#d4d4f0',
  textDim: '#8888b0',
  textMuted: '#55557a',

  // UI accents
  accent: '#7c6ff0',
  accentDim: '#5a50b0',
  green: '#6ee7a0',
  greenDim: '#3a8a5a',
  red: '#f07070',
  orange: '#f0a060',
  cyan: '#60d0f0',
  yellow: '#e8d060',
  pink: '#e070c0',

  // Syntax — One Dark inspired, tuned for contrast
  synKeyword:  '#c678dd',  // purple — for, if, end, function
  synBuiltin:  '#61afef',  // blue — plot, sin, zeros
  synNumber:   '#d19a66',  // orange — 42, 3.14, 1e-3
  synString:   '#98c379',  // green — 'hello'
  synComment:  '#5c6370',  // gray — % comment
  synOperator: '#abb2bf',  // light gray — + - * = ~
  synConstant: '#e5c07b',  // gold — pi, inf, true, false
  synParam:    '#56b6c2',  // teal — on, off, all, minor
};

const light = {
  name: 'light',

  // UI surfaces
  bg0: '#f5f5f7',
  bg1: '#ffffff',
  bg2: '#f0f0f5',
  bg3: '#e4e4ed',
  bg4: '#d8d8e5',
  border: '#ccccd8',
  borderHi: '#b0b0c4',

  // UI text
  text: '#1e1e2e',
  textDim: '#555570',
  textMuted: '#8888a0',

  // UI accents
  accent: '#6c5ce7',
  accentDim: '#8577ed',
  green: '#2d8659',
  greenDim: '#4aa872',
  red: '#d04040',
  orange: '#c07020',
  cyan: '#2090b0',
  yellow: '#a08000',
  pink: '#b050a0',

  // Syntax — One Light inspired
  synKeyword:  '#a626a4',  // purple
  synBuiltin:  '#4078f2',  // blue
  synNumber:   '#986801',  // brown
  synString:   '#50a14f',  // green
  synComment:  '#a0a1a7',  // gray
  synOperator: '#383a42',  // dark gray
  synConstant: '#c18401',  // gold
  synParam:    '#0184bc',  // teal
};

const themes = { dark, light };
let currentThemeName = 'dark';

export function getTheme() { return themes[currentThemeName]; }
export function getThemeByName(name) { return themes[name] || themes.dark; }
export function getThemeName() { return currentThemeName; }

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
  document.body.style.background = t.bg0;
  document.body.style.color = t.text;
}

// Default export for backward compatibility
const C = dark;
export default C;