/**
 * Reference panel data — merges three sources:
 *   1. Hand-written rich entries (signature, syntax, params, returns, examples, seeAlso)
 *   2. help.js — single-signature entries with category, description, one example
 *   3. cheatsheet.js — operator/syntax cheat sheet (rendered as pseudo-docs)
 *
 * Rich entries take precedence on name collision so the curated doc wins
 * over the auto-generated one.
 */

import HELP_DB from '../../data/help';
import CHEAT_SHEET from '../../data/cheatsheet';
import { REF_DOCS as RICH_DOCS } from './Reference';

function helpToDoc(name, e) {
  return {
    name,
    cat: e.cat || 'Misc',
    sig: e.sig,
    sum: e.desc,
    syntax: [e.sig],
    desc: e.desc,
    params: [],
    returns: [],
    examples: e.ex ? [e.ex] : [],
    seeAlso: [],
  };
}

function cheatToDocs(cs) {
  // Cheat-sheet sections become pseudo-docs grouped under "Cheat sheet" category
  return cs.flatMap((sec) =>
    sec.items.map((it) => ({
      name: it.code,
      cat: `Cheat: ${sec.title}`,
      sig: it.code,
      sum: it.desc,
      syntax: [it.code],
      desc: it.desc,
      params: [], returns: [], examples: [], seeAlso: [],
    }))
  );
}

const richNames = new Set(RICH_DOCS.map((d) => d.name));
const helpDocs = Object.entries(HELP_DB)
  .filter(([name]) => !richNames.has(name))
  .map(([name, e]) => helpToDoc(name, e));

export const ALL_DOCS = [
  ...RICH_DOCS,
  ...helpDocs,
  ...cheatToDocs(CHEAT_SHEET),
].sort((a, b) => {
  // Rich docs first, then alphabetical within each category bucket
  const aRich = richNames.has(a.name);
  const bRich = richNames.has(b.name);
  if (aRich !== bRich) return aRich ? -1 : 1;
  return a.name.localeCompare(b.name);
});
