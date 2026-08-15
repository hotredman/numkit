// ide/desktop/type-serialization.test.js
//
// Integration tests: JSON serialisation of every numkit Value type
// through the native --ide-session pipe protocol.
//
// Protocol mapping:
//   __INSPECT__:name        -> __VAR_DATA__: getVarDataJSON   -> {name,type,rows,cols,page,pages,data}
//   __GET_PAGE__:name\tpage -> __PAGE_DATA__: getVarDataJSON  -> same shape + page/pages
//   __GET_SHAPE__:name      -> __SHAPE_DATA__: getVarShapeJSON -> {name,type,rows,cols,ndim,pages,dims,numel}
//   __GET_STATS__:name\tp   -> __STATS_DATA__: getVarStatsJSON -> {rows,cols,n,min,max,mean,...}
//   __GET_TILE__:name\t...  -> __TILE_DATA__: getVarTileJSON   -> {r0,c0,rows,cols,type,data}
//   __INSPECT_PATH__:name\tp-> __PATH_DATA__: getInspectPathJSON -> {kind,type,rows,cols,data} | {kind,fields,elems} | {kind,elems}

import { describe, it, expect } from 'vitest';
import { spawn }    from 'node:child_process';
import path         from 'node:path';
import fs           from 'node:fs';
import { extractResponse } from './repl-protocol.js';

// == Locate binary ============================================================

const REPO_ROOT  = path.resolve(import.meta.dirname, '../..');
function findBinary() {
  const candidates = [
    path.join(REPO_ROOT, 'build/desktop-fast/apps/numkit/Release/numkit_repl.exe'),
    path.join(REPO_ROOT, 'build/desktop-fast/apps/numkit/numkit_repl'),
    path.join(REPO_ROOT, 'build/desktop/apps/numkit/Release/numkit_repl.exe'),
    path.join(REPO_ROOT, 'build/desktop/apps/numkit/numkit_repl'),
  ];
  return candidates.find(p => fs.existsSync(p)) ?? null;
}
const BINARY     = findBinary();
const HAS_BINARY = BINARY !== null;

// == Session helper ===========================================================

async function runAll(commands) {
  return new Promise((resolve, reject) => {
    if (!HAS_BINARY) { resolve([]); return; }
    const proc = spawn(BINARY, ['--ide-session'], { stdio: ['pipe', 'pipe', 'pipe'] });
    const results = [];
    let buf = '';
    let pending = null;

    proc.stdout.on('data', chunk => {
      buf += chunk.toString();
      // Drain all complete responses so we don't lose them between data events.
      while (true) {
        const { result, remainder } = extractResponse(buf);
        if (!result) break;
        buf = remainder;
        if (pending) {
          const cb = pending; pending = null;
          cb.resolve(result);
          break;   // wait for the async loop to send the next command
        } else {
          // spurious response (shouldn't happen in normal flow)
          results.push(result);
        }
      }
    });
    proc.on('error', reject);
    proc.on('close', () => resolve(results));

    async function execute() {
      for (const cmd of commands) {
        const r = await new Promise(res => {
          pending = { resolve: res };
          proc.stdin.write(cmd + '\n', 'utf8');
        });
        results.push(r);
      }
      proc.stdin.write('__QUIT__\n', 'utf8');
    }
    execute().catch(reject);
  });
}

// Convenience wrappers ---------------------------------------------------------

/** Run code then query; return query result's .data object. */
async function query(code, cmd) {
  const results = await runAll([`${code}\n__END_OF_INPUT__`, cmd]);
  const res = results[1];
  return res?.data ?? null;
}

// __INSPECT__:name  -> getVarDataJSON -> {name,type,rows,cols,page,pages,data} (NO kind field)
const getVarData  = (code, name)           => query(code, `__INSPECT__:${name}`);
// __GET_PAGE__:name\tpage -> same shape as getVarData
const getPage     = (code, name, page = 0) => query(code, `__GET_PAGE__:${name}\t${page}`);
// __GET_SHAPE__:name
const getShape    = (code, name)           => query(code, `__GET_SHAPE__:${name}`);
// __GET_STATS__:name\tpage
const getStats    = (code, name, page = -1)=> query(code, `__GET_STATS__:${name}\t${page}`);
// __GET_TILE__:name\tr0\tc0\trows\tcols\tpage
const getTile     = (code, name, r0, c0, rows, cols, pg = 0) =>
                      query(code, `__GET_TILE__:${name}\t${r0}\t${c0}\t${rows}\t${cols}\t${pg}`);
// __INSPECT_PATH__:name\tpath -> getInspectPathJSON -> typed payload WITH kind field
const inspectPath = (code, name, pathStr)  => query(code, `__INSPECT_PATH__:${name}\t${pathStr}`);

// =============================================================================
// 1. DOUBLE - scalar, vector, matrix, special values
//    __INSPECT__ / __GET_PAGE__ return: {name, type, rows, cols, page, pages, data}
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- double getVarData', () => {

  it('scalar: type=double, 1x1, numeric value', async () => {
    const d = await getVarData('x = 3.14;', 'x');
    expect(d.type).toBe('double');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(1);
    expect(d.data[0][0]).toBeCloseTo(3.14, 10);
  });

  it('includes name/page/pages metadata', async () => {
    const d = await getVarData('x = 7;', 'x');
    expect(d.name).toBe('x');
    expect(d.page).toBe(0);
    expect(d.pages).toBeGreaterThanOrEqual(1);
  });

  it('row vector: 1-row, N-col data', async () => {
    const d = await getVarData('v = [10 20 30];', 'v');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(3);
    expect(d.data[0]).toEqual([10, 20, 30]);
  });

  it('column vector: N-row, 1-col data', async () => {
    const d = await getVarData('v = [1; 2; 3];', 'v');
    expect(d.rows).toBe(3);
    expect(d.cols).toBe(1);
    expect(d.data.map(r => r[0])).toEqual([1, 2, 3]);
  });

  it('2x3 matrix: correct row-major layout', async () => {
    const d = await getVarData('M = [1 2 3; 4 5 6];', 'M');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(3);
    expect(d.data[0]).toEqual([1, 2, 3]);
    expect(d.data[1]).toEqual([4, 5, 6]);
  });

  it('NaN element -> JSON null', async () => {
    const d = await getVarData('x = NaN;', 'x');
    expect(d.data[0][0]).toBeNull();
  });

  it('+Inf -> "Inf"', async () => {
    const d = await getVarData('x = Inf;', 'x');
    expect(d.data[0][0]).toBe('Inf');
  });

  it('-Inf -> "-Inf"', async () => {
    const d = await getVarData('x = -Inf;', 'x');
    expect(d.data[0][0]).toBe('-Inf');
  });

  it('mixed NaN/Inf vector: each cell independent', async () => {
    const d = await getVarData('v = [1 NaN Inf -Inf 5];', 'v');
    expect(d.data[0][0]).toBe(1);
    expect(d.data[0][1]).toBeNull();
    expect(d.data[0][2]).toBe('Inf');
    expect(d.data[0][3]).toBe('-Inf');
    expect(d.data[0][4]).toBe(5);
  });
});

// =============================================================================
// 2. SINGLE
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- single', () => {
  it('scalar: type=single, correct value', async () => {
    const d = await getVarData('x = single(2.5);', 'x');
    expect(d.type).toBe('single');
    expect(d.data[0][0]).toBeCloseTo(2.5, 5);
  });

  it('single matrix: all values serialized', async () => {
    const d = await getVarData('M = single([1 2; 3 4]);', 'M');
    expect(d.type).toBe('single');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    expect(d.data[0][0]).toBe(1);
    expect(d.data[1][1]).toBe(4);
  });

  it('__GET_SHAPE__: type=single', async () => {
    const sh = await getShape('x = single(1);', 'x');
    expect(sh.type).toBe('single');
  });
});

// =============================================================================
// 3. INTEGER TYPES - all 8 variants
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- integer types', () => {
  const cases = [
    { type: 'int8',   expr: 'int8(-5)',          expected: -5          },
    { type: 'int16',  expr: 'int16(1000)',        expected: 1000        },
    { type: 'int32',  expr: 'int32(99999)',       expected: 99999       },
    { type: 'int64',  expr: 'int64(123456789)',   expected: 123456789   },
    { type: 'uint8',  expr: 'uint8(255)',         expected: 255         },
    { type: 'uint16', expr: 'uint16(65535)',      expected: 65535       },
    { type: 'uint32', expr: 'uint32(4294967295)', expected: 4294967295  },
    { type: 'uint64', expr: 'uint64(999999)',     expected: 999999      },
  ];

  for (const { type, expr, expected } of cases) {
    it(`${type} scalar: exact integer value`, async () => {
      const d = await getVarData(`x = ${expr};`, 'x');
      expect(d.type).toBe(type);
      expect(d.data[0][0]).toBe(expected);
    });
  }

  it('int32 2x2 matrix: row-major, all exact', async () => {
    const d = await getVarData('M = int32([10 20; 30 40]);', 'M');
    expect(d.type).toBe('int32');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    expect(d.data[0]).toEqual([10, 20]);
    expect(d.data[1]).toEqual([30, 40]);
  });

  it('uint8 vector: values [0,255]', async () => {
    const d = await getVarData('v = uint8([0 128 255]);', 'v');
    expect(d.type).toBe('uint8');
    expect(d.data[0]).toEqual([0, 128, 255]);
  });
});

// =============================================================================
// 4. LOGICAL
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- logical', () => {
  it('true scalar: JSON boolean true', async () => {
    const d = await getVarData('x = true;', 'x');
    expect(d.type).toBe('logical');
    expect(d.data[0][0]).toBe(true);
  });

  it('false scalar: JSON boolean false', async () => {
    const d = await getVarData('x = false;', 'x');
    expect(d.data[0][0]).toBe(false);
  });

  it('logical row vector: array of booleans', async () => {
    const d = await getVarData('v = logical([1 0 1 0]);', 'v');
    expect(d.type).toBe('logical');
    expect(d.data[0]).toEqual([true, false, true, false]);
  });

  it('logical matrix: 2D boolean array', async () => {
    const d = await getVarData('M = [true false; false true];', 'M');
    expect(d.type).toBe('logical');
    expect(d.data[0]).toEqual([true, false]);
    expect(d.data[1]).toEqual([false, true]);
  });

  it('comparison result is logical', async () => {
    const d = await getVarData('r = (3 > 2);', 'r');
    expect(d.type).toBe('logical');
    expect(d.data[0][0]).toBe(true);
  });

  it('__GET_SHAPE__: type=logical, correct dims', async () => {
    const sh = await getShape('v = true(1,6);', 'v');
    expect(sh.type).toBe('logical');
    expect(sh.rows).toBe(1);
    expect(sh.cols).toBe(6);
    expect(sh.numel).toBe(6);
  });
});

// =============================================================================
// 5. COMPLEX
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- complex', () => {
  it('scalar complex: type=complex, data is string with i', async () => {
    const d = await getVarData('z = 3 + 4i;', 'z');
    expect(d.type).toBe('complex');
    expect(typeof d.data[0][0]).toBe('string');
    expect(d.data[0][0]).toMatch(/3/);
    expect(d.data[0][0]).toMatch(/4/);
    expect(d.data[0][0]).toMatch(/i/);
  });

  it('negative imaginary: minus in string', async () => {
    const d = await getVarData('z = 1 - 2i;', 'z');
    expect(d.data[0][0]).toMatch(/1/);
    expect(d.data[0][0]).toMatch(/-/);
  });

  it('complex matrix: each cell is a string', async () => {
    const d = await getVarData('M = [1+2i, 3+4i; 5+6i, 7+8i];', 'M');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    for (const row of d.data)
      for (const v of row)
        expect(typeof v).toBe('string');
  });
});

// =============================================================================
// 6. CHAR / STRING - getVarData returns char array; inspectPath returns kind=matrix
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- char', () => {
  it('getVarData: type=char, data is char array', async () => {
    const d = await getVarData("s = 'hello';", 's');
    expect(d.type).toBe('char');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(5);
    expect(d.data[0]).toEqual(['h', 'e', 'l', 'l', 'o']);
  });

  it('single char: 1x1 char matrix', async () => {
    const d = await getVarData("c = 'A';", 'c');
    expect(d.type).toBe('char');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(1);
    expect(d.data[0][0]).toBe('A');
  });

  it('__GET_SHAPE__: type=char, numel=length', async () => {
    const sh = await getShape("s = 'world';", 's');
    expect(sh.type).toBe('char');
    expect(sh.rows).toBe(1);
    expect(sh.cols).toBe(5);
    expect(sh.numel).toBe(5);
  });

  it('inspectPath root: kind=matrix, type=char, data is char array', async () => {
    const d = await inspectPath("s = 'hello';", 's', '');
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('char');
    expect(d.data[0]).toEqual(['h', 'e', 'l', 'l', 'o']);
  });
});

// =============================================================================
// 7. __GET_SHAPE__ - cross-type
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- __GET_SHAPE__', () => {
  it('2D matrix: ndim, dims[], numel', async () => {
    const sh = await getShape('M = zeros(3,4);', 'M');
    expect(sh.name).toBe('M');
    expect(sh.type).toBe('double');
    expect(sh.rows).toBe(3);
    expect(sh.cols).toBe(4);
    expect(sh.numel).toBe(12);
    expect(sh.ndim).toBeGreaterThanOrEqual(2);
    expect(Array.isArray(sh.dims)).toBe(true);
    expect(sh.dims[0]).toBe(3);
    expect(sh.dims[1]).toBe(4);
  });

  it('struct: type=struct, rows/cols present', async () => {
    const sh = await getShape('s.x = 1; s.y = 2;', 's');
    expect(sh.type).toBe('struct');
    expect(sh.rows).toBe(1);
    expect(sh.cols).toBe(1);
  });

  it('cell: type=cell, numel correct', async () => {
    const sh = await getShape("c = {1, 'hi', [1 2 3]};", 'c');
    expect(sh.type).toBe('cell');
    expect(sh.rows).toBe(1);
    expect(sh.cols).toBe(3);
    expect(sh.numel).toBe(3);
  });

  it('unknown variable: returns error', async () => {
    const [sh] = await runAll(['__GET_SHAPE__:nonexistent_xyz']);
    expect(sh.data.error).toMatch(/not found/i);
  });
});

// =============================================================================
// 8. __GET_STATS__ - numeric types
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- __GET_STATS__', () => {
  it('double matrix: min/max/mean/n/hasNaN', async () => {
    const s = await getStats('M = [2 4 6; 8 10 12];', 'M');
    expect(s.min).toBe(2);
    expect(s.max).toBe(12);
    expect(s.mean).toBeCloseTo(7, 6);
    expect(s.n).toBe(6);
    expect(s.hasNaN).toBe(false);
  });

  it('matrix with NaN: hasNaN=true, NaN excluded', async () => {
    const s = await getStats('v = [1 NaN 3 NaN 5];', 'v');
    expect(s.hasNaN).toBe(true);
    expect(s.min).toBe(1);
    expect(s.max).toBe(5);
    expect(s.n).toBe(3);
  });

  it('int32: stats as exact integers', async () => {
    const s = await getStats('v = int32([10 20 30]);', 'v');
    expect(s.min).toBe(10);
    expect(s.max).toBe(30);
    expect(s.mean).toBeCloseTo(20, 6);
  });

  it('logical: stats treat true=1, false=0', async () => {
    const s = await getStats('v = logical([1 0 1 1 0]);', 'v');
    expect(s.min).toBe(0);
    expect(s.max).toBe(1);
    expect(s.mean).toBeCloseTo(0.6, 6);
  });

  it('single: stats for single precision', async () => {
    const s = await getStats('v = single([1 2 3 4]);', 'v');
    expect(s.min).toBe(1);
    expect(s.max).toBe(4);
    expect(s.mean).toBeCloseTo(2.5, 5);
  });

  it('struct: returns error (non-numeric)', async () => {
    const s = await getStats('st.x = 1;', 'st');
    expect(s.error).toBeTruthy();
  });
});

// =============================================================================
// 9. STRUCT - via inspectPath (returns kind field)
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- struct via inspectPath', () => {
  it('root path: kind=struct, has fields[], elems[]', async () => {
    const d = await inspectPath('s.x = 10; s.y = 20;', 's', '');
    expect(d.kind).toBe('struct');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(1);
    expect(d.numel).toBe(1);
    expect(d.fields).toContain('x');
    expect(d.fields).toContain('y');
    expect(Array.isArray(d.elems)).toBe(true);
    expect(d.elems).toHaveLength(1);
    expect(Array.isArray(d.elems[0])).toBe(true);
    expect(d.elems[0]).toHaveLength(2);
  });

  it('field cell has type/size/summary/bytes/drill', async () => {
    const d = await inspectPath('s.val = 42;', 's', '');
    const fi = d.fields.indexOf('val');
    const cell = d.elems[0][fi];
    expect(cell.type).toBe('double');
    expect(cell.size).toMatch(/1[x×]1/);
    expect(cell.bytes).toBeGreaterThan(0);
    expect(cell.drill).toBe(true);
    expect(typeof cell.summary).toBe('string');
  });

  it('string field: cell.type=char', async () => {
    const d = await inspectPath("s.name = 'Alice'; s.age = 30;", 's', '');
    const ni = d.fields.indexOf('name');
    expect(d.elems[0][ni].type).toBe('char');
  });

  it('matrix field: correct size in cell', async () => {
    const d = await inspectPath('s.M = [1 2 3; 4 5 6];', 's', '');
    const mi = d.fields.indexOf('M');
    expect(d.elems[0][mi].size).toMatch(/2[x×]3/);
  });

  it('nested struct field: cell.type=struct', async () => {
    const d = await inspectPath('s.inner.val = 1;', 's', '');
    const ii = d.fields.indexOf('inner');
    expect(d.elems[0][ii].type).toBe('struct');
  });

  it('struct array (1x3): numel=3, elems has 3 items', async () => {
    const d = await inspectPath('for i=1:3; sa(i).val = i*10; end', 'sa', '');
    expect(d.kind).toBe('struct');
    expect(d.numel).toBe(3);
    expect(d.elems).toHaveLength(3);
  });

  it('getVarData for struct: type=struct, data is fallback string', async () => {
    const d = await getVarData('s.x = 1;', 's');
    expect(d.type).toBe('struct');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(1);
    // data fallback for struct
    expect(Array.isArray(d.data)).toBe(true);
  });
});

// =============================================================================
// 10. CELL ARRAY - via inspectPath (returns kind field)
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- cell via inspectPath', () => {
  it('root path: kind=cell, rows=1, cols=3, 3 elems', async () => {
    const d = await inspectPath("c = {42, 'hello', [1 2 3]};", 'c', '');
    expect(d.kind).toBe('cell');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(3);
    expect(Array.isArray(d.elems)).toBe(true);
    expect(d.elems).toHaveLength(3);
  });

  it('cell elem labels are {row,col} format', async () => {
    const d = await inspectPath("c = {1, 2, 3};", 'c', '');
    expect(d.elems[0].label).toBe('{1,1}');
    expect(d.elems[1].label).toBe('{1,2}');
    expect(d.elems[2].label).toBe('{1,3}');
  });

  it('cell elem has type/size/bytes/drill', async () => {
    const d = await inspectPath("c = {42, 'hi'};", 'c', '');
    expect(d.elems[0].type).toBe('double');
    expect(d.elems[0].size).toMatch(/1[x×]1/);
    expect(d.elems[0].bytes).toBeGreaterThan(0);
    expect(d.elems[0].drill).toBe(true);
    expect(d.elems[1].type).toBe('char');
  });

  it('2x2 cell: 4 elems, {1,1} and {2,2} labels present', async () => {
    const d = await inspectPath("c = {1 2; 3 4};", 'c', '');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    expect(d.elems).toHaveLength(4);
    const labels = d.elems.map(e => e.label);
    expect(labels).toContain('{1,1}');
    expect(labels).toContain('{2,2}');
  });

  it('nested cell element: type=cell', async () => {
    const d = await inspectPath("c = {{1,2}, 'outer'};", 'c', '');
    expect(d.elems[0].type).toBe('cell');
  });

  it('cell with logical element: type=logical', async () => {
    const d = await inspectPath("c = {true, false};", 'c', '');
    expect(d.elems[0].type).toBe('logical');
    expect(d.elems[1].type).toBe('logical');
  });

  it('getVarData for cell: type=cell, data is fallback string', async () => {
    const d = await getVarData("c = {1, 'a'};", 'c');
    expect(d.type).toBe('cell');
    expect(d.rows).toBe(1);
    expect(d.cols).toBe(2);
    expect(Array.isArray(d.data)).toBe(true);
  });
});

// =============================================================================
// 11. __INSPECT_PATH__ drill-in
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- __INSPECT_PATH__ drill', () => {
  it('struct field f:name: kind=matrix, correct data', async () => {
    const d = await inspectPath('s.M = [1 2; 3 4];', 's', 'f:M');
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('double');
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
    expect(d.data[0]).toEqual([1, 2]);
  });

  it('nested struct f:outer;f:inner', async () => {
    const d = await inspectPath('outer.inner.val = 99;', 'outer', 'f:inner');
    expect(d.kind).toBe('struct');
    expect(d.fields).toContain('val');
  });

  it('deeply nested f:a;f:b;f:c: returns scalar', async () => {
    const d = await inspectPath('s.a.b.c = 7;', 's', 'f:a;f:b;f:c');
    expect(d.kind).toBe('matrix');
    expect(d.data[0][0]).toBe(7);
  });

  it('cell element c:0 (0-based): returns element payload', async () => {
    const d = await inspectPath("c = {[1 2 3], 'abc'};", 'c', 'c:0');
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('double');
    expect(d.data[0]).toEqual([1, 2, 3]);
  });

  it('cell element c:1: returns char payload', async () => {
    const d = await inspectPath("c = {42, 'hello'};", 'c', 'c:1');
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('char');
    expect(d.data[0]).toEqual(['h','e','l','l','o']);
  });

  it('cell contains struct: c:0 returns struct payload', async () => {
    const d = await inspectPath("c = {struct('x', 42, 'y', 7)};", 'c', 'c:0');
    expect(d.kind).toBe('struct');
    expect(d.fields).toContain('x');
  });

  it('struct field with int32 value: type=int32', async () => {
    const d = await inspectPath('s.val = int32(42);', 's', 'f:val');
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('int32');
    expect(d.data[0][0]).toBe(42);
  });

  it('struct field with logical value: booleans in data', async () => {
    const d = await inspectPath('s.flags = [true false true];', 's', 'f:flags');
    expect(d.kind).toBe('matrix');
    expect(d.type).toBe('logical');
    expect(d.data[0]).toEqual([true, false, true]);
  });

  it('invalid field name: returns error', async () => {
    const d = await inspectPath('s.a = 1;', 's', 'f:nonexistent_field_xyz');
    expect(d.error).toBeTruthy();
  });
});

// =============================================================================
// 12. 3D ARRAYS - shape + page navigation
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- 3D arrays', () => {
  it('__GET_SHAPE__: pages=N for 3D array', async () => {
    const sh = await getShape('A = zeros(2,3,4);', 'A');
    expect(sh.rows).toBe(2);
    expect(sh.cols).toBe(3);
    expect(sh.pages).toBe(4);
    expect(sh.numel).toBe(24);
  });

  it('__GET_PAGE__ page=0: first 2D slice', async () => {
    const d = await getPage('A = reshape(1:24, 2,3,4);', 'A', 0);
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(3);
    expect(d.page).toBe(0);
    expect(d.pages).toBe(4);
    expect(d.data).toHaveLength(2);
    expect(d.data[0]).toHaveLength(3);
  });

  it('__GET_PAGE__ page=3: last slice', async () => {
    const d = await getPage('A = reshape(1:24, 2,3,4);', 'A', 3);
    expect(d.page).toBe(3);
    expect(d.pages).toBe(4);
    expect(d.data).toHaveLength(2);
  });

  it('int32 3D array: page data contains exact integers', async () => {
    const d = await getPage('A = int32(reshape(1:8, 2,2,2));', 'A', 1);
    expect(d.type).toBe('int32');
    for (const row of d.data)
      for (const v of row)
        expect(Number.isInteger(v)).toBe(true);
  });

  it('logical 3D: page data contains booleans', async () => {
    const d = await getPage('A = reshape(logical([1 0 1 0 1 0 1 0]), 2,2,2);', 'A', 0);
    expect(d.type).toBe('logical');
    for (const row of d.data)
      for (const v of row)
        expect(typeof v).toBe('boolean');
  });
});

// =============================================================================
// 13. __GET_TILE__ - viewport tiles
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- __GET_TILE__', () => {
  it('tile at (0,0) 3x4: correct r0,c0,rows,cols,data', async () => {
    const d = await getTile('M = reshape(1:100, 10, 10);', 'M', 0, 0, 3, 4);
    expect(d.r0).toBe(0);
    expect(d.c0).toBe(0);
    expect(d.rows).toBe(3);
    expect(d.cols).toBe(4);
    expect(d.data).toHaveLength(3);
    expect(d.data[0]).toHaveLength(4);
  });

  it('tile with offset (5,3): r0/c0 reflected', async () => {
    const d = await getTile('M = reshape(1:100, 10, 10);', 'M', 5, 3, 2, 2);
    expect(d.r0).toBe(5);
    expect(d.c0).toBe(3);
    expect(d.rows).toBe(2);
    expect(d.cols).toBe(2);
  });

  it('int32 tile: values are integers', async () => {
    const d = await getTile('M = int32(magic(5));', 'M', 0, 0, 2, 2);
    expect(d.type).toBe('int32');
    for (const row of d.data)
      for (const v of row)
        expect(Number.isInteger(v)).toBe(true);
  });

  it('logical tile: values are booleans', async () => {
    const d = await getTile('M = eye(4) > 0;', 'M', 0, 0, 2, 2);
    expect(d.type).toBe('logical');
    expect(d.data[0][0]).toBe(true);
    expect(d.data[0][1]).toBe(false);
    expect(d.data[1][0]).toBe(false);
    expect(d.data[1][1]).toBe(true);
  });

  it('complex tile: values are strings', async () => {
    const d = await getTile('M = [1+2i 3+4i; 5+6i 7+8i];', 'M', 0, 0, 2, 2);
    expect(d.type).toBe('complex');
    for (const row of d.data)
      for (const v of row)
        expect(typeof v).toBe('string');
  });

  it('tile out of bounds: returns error', async () => {
    const d = await getTile('M = zeros(3,3);', 'M', 10, 10, 2, 2);
    expect(d.error).toBeTruthy();
  });
});

// =============================================================================
// 14. workspaceJSON (__VARS__) - all types appear after run
// =============================================================================

describe.skipIf(!HAS_BINARY)('type serialization -- workspaceJSON __VARS__', () => {
  it('all types appear with correct type field', async () => {
    const [res] = await runAll([[
      "d  = 3.14;",
      "s  = single(1);",
      "i  = int32(7);",
      "u  = uint8(255);",
      "lg = true;",
      "ch = 'hello';",
      "z  = 1+2i;",
      "st.x = 1;",
      "cl = {1, 2};",
    ].join('\n') + '\n__END_OF_INPUT__']);

    const v = res.vars;
    expect(v.d?.type).toBe('double');
    expect(v.s?.type).toBe('single');
    expect(v.i?.type).toBe('int32');
    expect(v.u?.type).toBe('uint8');
    expect(v.lg?.type).toBe('logical');
    expect(v.ch?.type).toBe('char');
    expect(v.z?.type).toBe('complex');
    expect(v.st?.type).toBe('struct');
    expect(v.cl?.type).toBe('cell');
  });

  it('double scalar: numeric preview', async () => {
    const [r] = await runAll(['x = 42;\n__END_OF_INPUT__']);
    expect(typeof r.vars.x.preview).toBe('number');
    expect(r.vars.x.preview).toBe(42);
  });

  it('double scalar: stats.min/max/mean present', async () => {
    const [r] = await runAll(['x = 5;\n__END_OF_INPUT__']);
    expect(r.vars.x.stats?.min).toBe(5);
    expect(r.vars.x.stats?.max).toBe(5);
    expect(r.vars.x.stats?.mean).toBe(5);
  });

  it('char: string preview', async () => {
    const [r] = await runAll(["s = 'world';\n__END_OF_INPUT__"]);
    expect(typeof r.vars.s.preview).toBe('string');
    expect(r.vars.s.preview).toBe('world');
  });

  it('struct: null preview', async () => {
    const [r] = await runAll(['st.x = 1;\n__END_OF_INPUT__']);
    expect(r.vars.st.preview).toBeNull();
  });

  it('cell: null preview', async () => {
    const [r] = await runAll(["cl = {1, 'a'};\n__END_OF_INPUT__"]);
    expect(r.vars.cl.preview).toBeNull();
  });

  it('double array (<=10 elems): array preview', async () => {
    const [r] = await runAll(['v = [1 2 3];\n__END_OF_INPUT__']);
    expect(Array.isArray(r.vars.v.preview)).toBe(true);
    expect(r.vars.v.preview).toEqual([1, 2, 3]);
  });

  it('logical scalar: boolean preview', async () => {
    const [r] = await runAll(['x = true;\n__END_OF_INPUT__']);
    expect(r.vars.x.preview).toBe(true);
  });

  it('vars has size field in "RxC" format', async () => {
    const [r] = await runAll(['M = zeros(3,4);\n__END_OF_INPUT__']);
    expect(r.vars.M?.size).toMatch(/3[x×]4/);
  });

  it('vars has bytes field (positive integer)', async () => {
    const [r] = await runAll(['x = rand(10,10);\n__END_OF_INPUT__']);
    expect(typeof r.vars.x.bytes).toBe('number');
    expect(r.vars.x.bytes).toBeGreaterThan(0);
  });
});
