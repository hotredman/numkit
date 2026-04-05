import HELP_DB from './data/help';

export function createInterpreter() {
  let vars = {};
  let plotData = null;
  let plotConfig = { title: "", xlabel: "", ylabel: "", type: "line" };

  const mathFns = {
    sin: Math.sin, cos: Math.cos, tan: Math.tan,
    asin: Math.asin, acos: Math.acos, atan: Math.atan,
    sqrt: Math.sqrt, abs: Math.abs, exp: Math.exp,
    log: Math.log, log2: Math.log2, log10: Math.log10,
    floor: Math.floor, ceil: Math.ceil, round: Math.round,
    sign: Math.sign,
  };

  function parseVector(s) {
    // try parsing [1 2 3] or [1,2,3] or 1:10 or linspace(...)
    s = s.trim();
    if (s.startsWith("[") && s.endsWith("]")) {
      const inner = s.slice(1, -1).trim();
      if (inner.includes(";")) {
        // matrix
        return inner.split(";").map((row) =>
          row.trim().split(/[\s,]+/).map(Number)
        );
      }
      return inner.split(/[\s,]+/).map(Number);
    }
    // range a:b or a:step:b
    const rangeParts = s.split(":");
    if (rangeParts.length === 2) {
      const [a, b] = rangeParts.map(Number);
      const arr = [];
      for (let i = a; i <= b; i++) arr.push(i);
      return arr;
    }
    if (rangeParts.length === 3) {
      const [a, step, b] = rangeParts.map(Number);
      const arr = [];
      for (let i = a; step > 0 ? i <= b : i >= b; i += step) arr.push(i);
      return arr;
    }
    return null;
  }

  function evalExpr(expr) {
    expr = expr.trim();
    // Remove trailing semicolon
    if (expr.endsWith(";")) expr = expr.slice(0, -1).trim();

    // Constants
    if (expr === "pi") return Math.PI;
    if (expr === "inf") return Infinity;
    if (expr === "nan") return NaN;
    if (expr === "eps") return Number.EPSILON;
    if (expr === "true") return 1;
    if (expr === "false") return 0;

    // Number
    if (/^-?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(expr)) return parseFloat(expr);

    // Complex number
    if (/^-?\d+(\.\d+)?\s*[+-]\s*\d+(\.\d+)?i$/.test(expr)) return expr;

    // String
    if (/^'[^']*'$/.test(expr)) return expr.slice(1, -1);

    // Variable
    if (/^[a-zA-Z_]\w*$/.test(expr) && expr in vars) return vars[expr];

    // Vector/matrix literal
    const vec = parseVector(expr);
    if (vec) return vec;

    // linspace
    let m = expr.match(/^linspace\((.+),(.+),(.+)\)$/);
    if (m) {
      const a = evalExpr(m[1]), b = evalExpr(m[2]), n = evalExpr(m[3]);
      const arr = [];
      for (let i = 0; i < n; i++) arr.push(a + (b - a) * i / (n - 1));
      return arr;
    }

    // rand
    m = expr.match(/^rand\((\d+)(?:,\s*(\d+))?\)$/);
    if (m) {
      const rows = parseInt(m[1]), cols = m[2] ? parseInt(m[2]) : rows;
      if (rows === 1) return Array.from({ length: cols }, () => Math.random());
      return Array.from({ length: rows }, () => Array.from({ length: cols }, () => Math.random()));
    }

    // randn (Box-Muller)
    m = expr.match(/^randn\((\d+)(?:,\s*(\d+))?\)$/);
    if (m) {
      const rows = parseInt(m[1]), cols = m[2] ? parseInt(m[2]) : rows;
      const bm = () => { const u1 = Math.random(), u2 = Math.random(); return Math.sqrt(-2*Math.log(u1))*Math.cos(2*Math.PI*u2); };
      if (rows === 1) return Array.from({ length: cols }, bm);
      return Array.from({ length: rows }, () => Array.from({ length: cols }, bm));
    }

    // zeros/ones/eye
    m = expr.match(/^(zeros|ones|eye)\((\d+)(?:,\s*(\d+))?\)$/);
    if (m) {
      const fn = m[1], r = parseInt(m[2]), c = m[3] ? parseInt(m[3]) : r;
      if (fn === "zeros") { if (r === 1) return Array(c).fill(0); return Array.from({length:r}, () => Array(c).fill(0)); }
      if (fn === "ones") { if (r === 1) return Array(c).fill(1); return Array.from({length:r}, () => Array(c).fill(1)); }
      if (fn === "eye") return Array.from({length:r}, (_, i) => Array.from({length:c}, (_, j) => i===j?1:0));
    }

    // Math functions on arrays/scalars
    for (const [name, fn] of Object.entries(mathFns)) {
      const re = new RegExp(`^${name}\\((.+)\\)$`);
      m = expr.match(re);
      if (m) {
        const arg = evalExpr(m[1]);
        if (Array.isArray(arg)) return arg.map(fn);
        return fn(arg);
      }
    }

    // sum, prod, mean, min, max, length, numel
    const aggFns = { sum: (a) => a.flat().reduce((s,v)=>s+v,0), prod: (a) => a.flat().reduce((s,v)=>s*v,1), mean: (a) => { const f = a.flat(); return f.reduce((s,v)=>s+v,0)/f.length; }, min: (a) => Math.min(...a.flat()), max: (a) => Math.max(...a.flat()), length: (a) => Array.isArray(a)?a.length:1, numel: (a) => Array.isArray(a)?a.flat().length:1, cumsum: (a) => { let s=0; return a.map(v => s+=v); }, sort: (a) => [...a].sort((x,y)=>x-y) };
    for (const [name, fn] of Object.entries(aggFns)) {
      const re = new RegExp(`^${name}\\((.+)\\)$`);
      m = expr.match(re);
      if (m) {
        let arg = evalExpr(m[1]);
        if (!Array.isArray(arg)) arg = [arg];
        return fn(arg);
      }
    }

    // mod, rem, pow
    m = expr.match(/^(mod|rem)\((.+),(.+)\)$/);
    if (m) { const a = evalExpr(m[2]), b = evalExpr(m[3]); return a % b; }
    m = expr.match(/^pow\((.+),(.+)\)$/);
    if (m) { return Math.pow(evalExpr(m[1]), evalExpr(m[2])); }

    // upper, lower, strcmp
    m = expr.match(/^upper\((.+)\)$/);
    if (m) { return String(evalExpr(m[1])).toUpperCase(); }
    m = expr.match(/^lower\((.+)\)$/);
    if (m) { return String(evalExpr(m[1])).toLowerCase(); }
    m = expr.match(/^strcmp\((.+),(.+)\)$/);
    if (m) { return evalExpr(m[1]) === evalExpr(m[2]) ? 1 : 0; }
    m = expr.match(/^num2str\((.+)\)$/);
    if (m) { return String(evalExpr(m[1])); }

    // real, imag, abs for complex
    m = expr.match(/^(real|imag|conj)\((.+)\)$/);
    if (m) {
      const arg = evalExpr(m[2]);
      if (typeof arg === "number") {
        if (m[1] === "real") return arg;
        if (m[1] === "imag") return 0;
        return arg;
      }
    }

    // size
    m = expr.match(/^size\((.+)\)$/);
    if (m) {
      const arg = evalExpr(m[1]);
      if (Array.isArray(arg) && Array.isArray(arg[0])) return [arg.length, arg[0].length];
      if (Array.isArray(arg)) return [1, arg.length];
      return [1, 1];
    }

    // isempty, isnumeric, ischar
    m = expr.match(/^isempty\((.+)\)$/);
    if (m) { const a = evalExpr(m[1]); return (Array.isArray(a) && a.length===0) ? 1 : 0; }
    m = expr.match(/^isnumeric\((.+)\)$/);
    if (m) { return typeof evalExpr(m[1]) === "number" ? 1 : 0; }
    m = expr.match(/^ischar\((.+)\)$/);
    if (m) { return typeof evalExpr(m[1]) === "string" ? 1 : 0; }

    // Simple arithmetic with ^ support
    try {
      let safeExpr = expr
        .replace(/\bpi\b/g, String(Math.PI))
        .replace(/\binf\b/g, "Infinity")
        .replace(/\beps\b/g, String(Number.EPSILON))
        .replace(/\btrue\b/g, "1")
        .replace(/\bfalse\b/g, "0");
      // Replace variable names
      for (const [k, v] of Object.entries(vars)) {
        if (typeof v === "number") {
          safeExpr = safeExpr.replace(new RegExp(`\\b${k}\\b`, "g"), String(v));
        }
      }
      // Replace ^ with **
      safeExpr = safeExpr.replace(/\^/g, "**");
      // Only allow safe chars
      if (/^[\d\s+\-*/().,%eE<>=!&|~?:]+$/.test(safeExpr)) {
        const result = Function(`"use strict"; return (${safeExpr})`)();
        if (typeof result === "number") return result;
      }
    } catch (e) { /* fall through */ }

    return undefined;
  }

  function formatVal(v) {
    if (v === undefined || v === null) return "";
    if (typeof v === "string") return v;
    if (typeof v === "number") {
      if (Number.isInteger(v)) return String(v);
      return v.toPrecision(6).replace(/\.?0+$/, "") || "0";
    }
    if (Array.isArray(v)) {
      if (v.length && Array.isArray(v[0])) {
        // Matrix
        const strs = v.map((row) => row.map((x) => formatVal(x)));
        const maxW = strs.reduce(
          (acc, row) => row.map((s, i) => Math.max(acc[i] || 0, s.length)),
          []
        );
        return strs
          .map((row) => row.map((s, i) => s.padStart(maxW[i])).join("  "))
          .join("\n");
      }
      return v.map(formatVal).join("  ");
    }
    return String(v);
  }

  function execute(code) {
    const lines = code.split("\n");
    const output = [];
    plotData = null;

    // Function definitions: collect them
    const funcDefs = {};
    let i = 0;
    while (i < lines.length) {
      const line = lines[i].trim();
      if (line.startsWith("function ")) {
        // Collect until matching end
        let depth = 1;
        let body = [line];
        i++;
        while (i < lines.length && depth > 0) {
          const l = lines[i].trim();
          const first = l.split(/[\s(;,]+/)[0];
          if (["for", "while", "if", "switch", "try", "function"].includes(first)) depth++;
          if (/\bend\b/.test(l)) depth--;
          body.push(lines[i]);
          i++;
        }
        // Parse function name
        const fm = line.match(/function\s+(?:\[?\w+(?:\s*,\s*\w+)*\]?\s*=\s*)?(\w+)\s*\(/);
        if (fm) funcDefs[fm[1]] = body.join("\n");
        continue;
      }
      i++;
    }

    // Execute non-function lines
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith("%") || trimmed.startsWith("function ")) continue;
      if (trimmed === "end") continue;

      const suppressOutput = trimmed.endsWith(";");
      const cleanLine = trimmed.replace(/;$/, "").trim();

      // clc
      if (cleanLine === "clc") { output.push("__CLEAR__"); continue; }

      // clear
      if (cleanLine === "clear") { vars = {}; output.push("Workspace cleared."); continue; }

      // who / whos
      if (cleanLine === "who" || cleanLine === "whos") {
        const keys = Object.keys(vars);
        if (!keys.length) { output.push("No variables in workspace."); continue; }
        if (cleanLine === "who") { output.push(keys.join("  ")); continue; }
        const lines2 = ["  Name          Size        Class"];
        for (const k of keys) {
          const v = vars[k];
          let sz = "1x1", cls = typeof v;
          if (Array.isArray(v)) {
            if (v.length && Array.isArray(v[0])) sz = `${v.length}x${v[0].length}`;
            else sz = `1x${v.length}`;
            cls = "double";
          } else if (typeof v === "string") cls = "char";
          else cls = "double";
          lines2.push(`  ${k.padEnd(14)}${sz.padEnd(12)}${cls}`);
        }
        output.push(lines2.join("\n"));
        continue;
      }

      // disp
      let dm = cleanLine.match(/^disp\((.+)\)$/);
      if (dm) {
        const val = evalExpr(dm[1]);
        if (val !== undefined) output.push(formatVal(val));
        else output.push(`Error: undefined expression '${dm[1]}'`);
        continue;
      }

      // fprintf / sprintf
      dm = cleanLine.match(/^fprintf\((.+)\)$/);
      if (dm) {
        // Simplified: just eval and display
        output.push(String(evalExpr(dm[1]) || dm[1].replace(/^'|'$/g, "")));
        continue;
      }

      // Plot commands
      dm = cleanLine.match(/^plot\((.+)\)$/);
      if (dm) {
        const args = dm[1].split(/,(?![^(]*\))/);
        if (args.length >= 2) {
          const x = evalExpr(args[0].trim());
          const y = evalExpr(args[1].trim());
          if (Array.isArray(x) && Array.isArray(y)) {
            if (!plotData) plotData = { datasets: [], config: { ...plotConfig, type: "line" } };
            plotData.datasets.push({ x, y });
          }
        } else {
          const y = evalExpr(args[0].trim());
          if (Array.isArray(y)) {
            const x = y.map((_, i) => i + 1);
            if (!plotData) plotData = { datasets: [], config: { ...plotConfig, type: "line" } };
            plotData.datasets.push({ x, y });
          }
        }
        continue;
      }

      dm = cleanLine.match(/^bar\((.+)\)$/);
      if (dm) {
        const args = dm[1].split(/,(?![^(]*\))/);
        const y = evalExpr(args[0].trim());
        if (Array.isArray(y)) {
          const x = y.map((_, i) => i + 1);
          plotData = { datasets: [{ x, y }], config: { ...plotConfig, type: "bar" } };
        }
        continue;
      }

      dm = cleanLine.match(/^scatter\((.+),(.+)\)$/);
      if (dm) {
        const x = evalExpr(dm[1].trim());
        const y = evalExpr(dm[2].trim());
        if (Array.isArray(x) && Array.isArray(y)) {
          plotData = { datasets: [{ x, y }], config: { ...plotConfig, type: "scatter" } };
        }
        continue;
      }

      dm = cleanLine.match(/^hist\((.+?)(?:,\s*(\d+))?\)$/);
      if (dm) {
        const data = evalExpr(dm[1].trim());
        const bins = dm[2] ? parseInt(dm[2]) : 10;
        if (Array.isArray(data)) {
          const mn = Math.min(...data), mx = Math.max(...data);
          const bw = (mx - mn) / bins;
          const counts = Array(bins).fill(0);
          const centers = [];
          for (let b = 0; b < bins; b++) centers.push(mn + bw * (b + 0.5));
          for (const v of data) {
            let b = Math.floor((v - mn) / bw);
            if (b >= bins) b = bins - 1;
            if (b < 0) b = 0;
            counts[b]++;
          }
          plotData = { datasets: [{ x: centers, y: counts }], config: { ...plotConfig, type: "bar" } };
        }
        continue;
      }

      // title, xlabel, ylabel
      dm = cleanLine.match(/^title\('(.+)'\)$/);
      if (dm) { plotConfig.title = dm[1]; if (plotData) plotData.config.title = dm[1]; continue; }
      dm = cleanLine.match(/^xlabel\('(.+)'\)$/);
      if (dm) { plotConfig.xlabel = dm[1]; if (plotData) plotData.config.xlabel = dm[1]; continue; }
      dm = cleanLine.match(/^ylabel\('(.+)'\)$/);
      if (dm) { plotConfig.ylabel = dm[1]; if (plotData) plotData.config.ylabel = dm[1]; continue; }

      // help
      dm = cleanLine.match(/^help\s+(\w+)$/);
      if (dm) {
        const topic = dm[1];
        const info = HELP_DB[topic];
        if (info) {
          output.push(`  ${info.sig}\n  ${info.desc}\n  Category: ${info.cat}\n  Example: ${info.ex}`);
        } else {
          output.push(`No help available for '${topic}'.`);
        }
        continue;
      }
      if (cleanLine === "help") {
        output.push("Commands: clc, clear, who, whos, help <topic>\nKeys: Enter=exec, Shift+Enter=newline, Tab=autocomplete, ↑↓=history");
        continue;
      }

      // Assignment
      dm = cleanLine.match(/^(\w+(?:\.\w+)*)\s*=\s*(.+)$/);
      if (dm) {
        const name = dm[1];
        const val = evalExpr(dm[2]);
        if (val !== undefined) {
          // Handle struct fields
          if (name.includes(".")) {
            const parts = name.split(".");
            let obj = vars[parts[0]] || {};
            vars[parts[0]] = obj;
            for (let p = 1; p < parts.length - 1; p++) {
              if (!obj[parts[p]]) obj[parts[p]] = {};
              obj = obj[parts[p]];
            }
            obj[parts[parts.length - 1]] = val;
          } else {
            vars[name] = val;
          }
          if (!suppressOutput) output.push(`${name} = ${formatVal(val)}`);
        } else {
          output.push(`Error: cannot evaluate '${dm[2]}'`);
        }
        continue;
      }

      // Expression
      const val = evalExpr(cleanLine);
      if (val !== undefined) {
        if (!suppressOutput) output.push(`ans = ${formatVal(val)}`);
        vars.ans = val;
      } else if (cleanLine.length > 0 && !["for","while","if","elseif","else","switch","case","otherwise","try","catch","break","continue","return"].includes(cleanLine.split(/\s/)[0])) {
        output.push(`[fallback] ${cleanLine}`);
      }
    }

    return { output: output.join("\n"), plot: plotData };
  }

  function getVars() { return { ...vars }; }
  function reset() {
    vars = {};
    plotData = null;
    plotConfig = { title: "", xlabel: "", ylabel: "", type: "line" };
  }

  function complete(partial) {
    const kw = Object.keys(HELP_DB).concat(["for", "while", "if", "else", "elseif", "end", "function", "return", "break", "continue", "switch", "case", "otherwise", "try", "catch", "classdef", "global", "true", "false", "pi", "inf", "nan", "eps"]);
    return kw.filter((k) => k.startsWith(partial));
  }

  return { execute, getVars, reset, complete };
}
