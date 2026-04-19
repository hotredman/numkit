/**
 * theme.js — Single source of truth for numkit mIDE theming.
 *
 * Usage in components:
 *   import { useTheme, FONT, FONT_UI } from '../theme';
 *   function MyComponent() {
 *     const C = useTheme();
 *     return <div style={{ color: C.text }}>...</div>;
 *   }
 *
 * Theme toggle:
 *   import { ThemeProvider } from '../theme';
 *   <ThemeProvider>
 *     <App />
 *   </ThemeProvider>
 */

import { createContext, useContext, useState, useEffect, useCallback } from 'react';

export const FONT = "'IBM Plex Mono', 'JetBrains Mono', 'Fira Code', 'SF Mono', Consolas, monospace";
export const FONT_UI = "'DM Sans', 'IBM Plex Mono', sans-serif";

// ── Dark theme ──
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
  synKeyword:  '#c678dd',
  synBuiltin:  '#61afef',
  synNumber:   '#d19a66',
  synString:   '#98c379',
  synComment:  '#5c6370',
  synOperator: '#abb2bf',
  synConstant: '#e5c07b',
  synParam:    '#56b6c2',
};

// ── Light theme ──
const light = {
  name: 'light',
  bg0: '#f0f0f4',
  bg1: '#fafafe',
  bg2: '#eeeef4',
  bg3: '#e0e0ea',
  bg4: '#d4d4e0',
  border: '#c8c8d8',
  borderHi: '#a8a8c0',
  text: '#1e1e2e',
  textDim: '#4a4a68',
  textMuted: '#7a7a98',
  accent: '#6c5ce7',
  accentDim: '#8577ed',
  green: '#1a7a42',
  greenDim: '#2d9956',
  red: '#c03030',
  orange: '#b06018',
  cyan: '#1878a0',
  yellow: '#8a7000',
  pink: '#a040a0',
  synKeyword:  '#a626a4',
  synBuiltin:  '#4078f2',
  synNumber:   '#986801',
  synString:   '#50a14f',
  synComment:  '#a0a1a7',
  synOperator: '#383a42',
  synConstant: '#c18401',
  synParam:    '#0184bc',
};

const themes = { dark, light };

// ── React Context ──
const ThemeContext = createContext(dark);

export function ThemeProvider({ children }) {
  const [themeName, setThemeName] = useState(() => {
    try { return localStorage.getItem('mlab-theme') || 'dark'; } catch { return 'dark'; }
  });

  const theme = themes[themeName] || dark;

  // Apply CSS variables whenever theme changes
  useEffect(() => {
    const root = document.documentElement;
    for (const [key, val] of Object.entries(theme)) {
      if (key === 'name') continue;
      root.style.setProperty(`--c-${key}`, val);
    }
    root.style.setProperty('--font-mono', FONT);
    root.style.setProperty('--font-ui', FONT_UI);
    document.body.style.background = theme.bg0;
    document.body.style.color = theme.text;
    try { localStorage.setItem('mlab-theme', themeName); } catch {}
  }, [theme, themeName]);

  const toggle = useCallback(() => {
    setThemeName(prev => prev === 'dark' ? 'light' : 'dark');
  }, []);

  return (
    <ThemeContext.Provider value={{ ...theme, themeName, toggleTheme: toggle }}>
      {children}
    </ThemeContext.Provider>
  );
}

/** Hook: use in any component to get current theme colors + toggleTheme() */
export function useTheme() {
  return useContext(ThemeContext);
}

// Default export for backward compatibility (static dark)
export default dark;
