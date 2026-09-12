// Pure legend helper (R5): re-export the color decode so the legend
// component stays markup-only.
import { seriesColor } from './plotModel.js';
export { T } from '../tokens.js';
export const seriesColorOf = (style, idx) => seriesColor(style, idx);
