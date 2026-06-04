// ide/src/components/lang/astShared.js
//
// Shared helpers / config for both AST views (graph and tree). Kept
// in one place so adding a new NodeType / category is a one-file
// change instead of a hunt-and-update across two renderers.

/** NodeType taxonomy → display category. Order here drives the order
 *  of chips in the filter bar (left → right). */
export const CATEGORIES = [
  {
    key:    'container',
    label:  'Containers',
    types:  ['BLOCK', 'EXPR_STMT'],
    color:  '#7a8390',
    // ON by default — BLOCK is the script root; filtering it would
    // turn the rendered structure into a forest of orphan top-level
    // nodes, which works in `layered` graph layout but is confusing
    // in the tree view (multiple roots at depth 0).
    defaultOn: true,
  },
  { key: 'literal',  label: 'Literals',
    types: ['NUMBER_LITERAL', 'IMAG_LITERAL', 'STRING_LITERAL',
            'DQSTRING_LITERAL', 'BOOL_LITERAL', 'MATRIX_LITERAL',
            'CELL_LITERAL'],
    color: '#7fd99a', defaultOn: true },
  { key: 'ident',    label: 'Identifiers',
    types: ['IDENTIFIER', 'END_VAL'],
    color: '#7fd0e0', defaultOn: true },
  { key: 'operator', label: 'Operators',
    types: ['BINARY_OP', 'UNARY_OP', 'COLON_EXPR'],
    color: '#9b8cf2', defaultOn: true },
  { key: 'access',   label: 'Calls & access',
    types: ['CALL', 'COMMAND_CALL', 'INDEX', 'CELL_INDEX',
            'FIELD_ACCESS', 'DYNAMIC_FIELD_ACCESS', 'ANON_FUNC'],
    color: '#f0b97a', defaultOn: true },
  { key: 'assign',   label: 'Assignments',
    types: ['ASSIGN', 'MULTI_ASSIGN', 'DELETE_ASSIGN'],
    color: '#5fb87a', defaultOn: true },
  { key: 'control',  label: 'Control flow',
    types: ['IF_STMT', 'FOR_STMT', 'WHILE_STMT', 'SWITCH_STMT',
            'BREAK_STMT', 'CONTINUE_STMT', 'RETURN_STMT', 'TRY_STMT'],
    color: '#d97c7c', defaultOn: true },
  { key: 'decl',     label: 'Declarations',
    types: ['FUNCTION_DEF', 'GLOBAL_STMT', 'PERSISTENT_STMT'],
    color: '#c98cf2', defaultOn: true },
];

export const TYPE_TO_CAT = (() => {
  const m = new Map();
  for (const cat of CATEGORIES) for (const t of cat.types) m.set(t, cat.key);
  return m;
})();

export const categoryOf  = (t) => TYPE_TO_CAT.get(t) || 'other';
export const categoryColor = (key) => {
  const cat = CATEGORIES.find((c) => c.key === key);
  return cat?.color || '#888';
};

export function defaultFilters() {
  const f = {};
  for (const cat of CATEGORIES) f[cat.key] = cat.defaultOn;
  f.other = true;
  return f;
}

/** True iff the AST node has at least one descendant (children,
 *  branches, or elseBranch). Used to decide whether to show the
 *  collapse chevron. */
export function hasAnyChildren(node) {
  if (!node) return false;
  if (node.children && node.children.length > 0) return true;
  if (node.branches && node.branches.length > 0) return true;
  if (node.elseBranch) return true;
  return false;
}

/** Compact value preview shown beside the type chip (e.g. `"x"` for
 *  IDENTIFIER, `3.14` for NUMBER_LITERAL). Returns empty string
 *  when the node carries no useful inline value. */
export function valueText(node) {
  if (!node) return '';
  switch (node.type) {
    case 'IDENTIFIER':           return node.strValue || '';
    case 'NUMBER_LITERAL':       return String(node.numValue ?? '');
    case 'IMAG_LITERAL':         return `${node.numValue ?? 0}i`;
    case 'STRING_LITERAL':       return `'${node.strValue ?? ''}'`;
    case 'DQSTRING_LITERAL':     return `"${node.strValue ?? ''}"`;
    case 'BOOL_LITERAL':         return node.boolValue ? 'true' : 'false';
    case 'BINARY_OP':
    case 'UNARY_OP':             return node.strValue || '';
    case 'FUNCTION_DEF':         return node.strValue || '';
    case 'CALL':
    case 'COMMAND_CALL':         return node.strValue || '';
    case 'FIELD_ACCESS':
    case 'DYNAMIC_FIELD_ACCESS': return `.${node.strValue || ''}`;
    case 'FOR_STMT':             return node.strValue || '';   // iter var
    case 'TRY_STMT':             return node.strValue || '';   // catch var
    default:                     return '';
  }
}

/** Walk every child slot of an AST node, calling `cb(child, path)`
 *  with the path-id we'd use to render it. Centralises the iteration
 *  logic so the renderers don't reinvent `children` / `branches` /
 *  `elseBranch` enumeration. */
export function eachAstChild(node, path, cb) {
  if (!node) return;
  const kids = node.children || [];
  for (let i = 0; i < kids.length; ++i) cb(kids[i], `${path}/c${i}`);
  const branches = node.branches || [];
  for (let i = 0; i < branches.length; ++i) {
    cb(branches[i].cond, `${path}/b${i}/cond`);
    cb(branches[i].body, `${path}/b${i}/body`);
  }
  if (node.elseBranch) cb(node.elseBranch, `${path}/else`);
}

/** Find the deepest VISIBLE node whose source range contains
 *  `cursorLine`. Used by both AST renderers for the editor → AST
 *  highlight handoff. Returns the path-id or null. */
export function findActiveAstId(astRoot, cursorLine, collapsedSet, filters) {
  if (!astRoot || !cursorLine || cursorLine < 1) return null;
  let best = null;
  function rangeContains(node) {
    const startL = node.line     || 0;
    const endL   = node.endLine  || startL;
    return cursorLine >= startL && cursorLine <= endL;
  }
  function visit(node, path) {
    if (!node || !rangeContains(node)) return;
    const visible = filters[categoryOf(node.type)] !== false;
    if (visible) best = path;
    if (collapsedSet.has(path)) return;
    eachAstChild(node, path, (child, childPath) => visit(child, childPath));
  }
  visit(astRoot, '0');
  return best;
}

/** Collect path-IDs of every VISIBLE node that has children — i.e.
 *  the set "collapse all" should mark as collapsed. */
export function collectCollapsibleIds(astRoot, filters) {
  const out = new Set();
  if (!astRoot) return out;
  function visit(node, path) {
    if (!node) return;
    const visible = filters[categoryOf(node.type)] !== false;
    if (visible && hasAnyChildren(node)) out.add(path);
    eachAstChild(node, path, visit);
  }
  visit(astRoot, '0');
  return out;
}
