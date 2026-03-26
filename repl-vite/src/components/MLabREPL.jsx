import { useState, useEffect, useRef, useCallback, useMemo } from "react";
import * as d3 from "d3";

// ════════════════════════════════════════════════════════════════
// MLab REPL — Enhanced Web IDE
// Features: Terminal, Plotting, File I/O, Tabs, Variable Inspector,
//           Examples, Cheat Sheet, Help System
// ════════════════════════════════════════════════════════════════

// ── Help Database ──
const HELP_DB = {
  // Math
  sin: { sig: "sin(x)", desc: "Sine of x (radians)", cat: "Math", ex: "sin(pi/2)  % → 1" },
  cos: { sig: "cos(x)", desc: "Cosine of x (radians)", cat: "Math", ex: "cos(0)  % → 1" },
  tan: { sig: "tan(x)", desc: "Tangent of x (radians)", cat: "Math", ex: "tan(pi/4)  % → 1" },
  asin: { sig: "asin(x)", desc: "Inverse sine, result in radians", cat: "Math", ex: "asin(1)  % → pi/2" },
  acos: { sig: "acos(x)", desc: "Inverse cosine, result in radians", cat: "Math", ex: "acos(1)  % → 0" },
  atan: { sig: "atan(x)", desc: "Inverse tangent, result in radians", cat: "Math", ex: "atan(1)  % → pi/4" },
  sqrt: { sig: "sqrt(x)", desc: "Square root of x", cat: "Math", ex: "sqrt(144)  % → 12" },
  abs: { sig: "abs(x)", desc: "Absolute value of x", cat: "Math", ex: "abs(-7)  % → 7" },
  exp: { sig: "exp(x)", desc: "Exponential e^x", cat: "Math", ex: "exp(1)  % → 2.7183" },
  log: { sig: "log(x)", desc: "Natural logarithm of x", cat: "Math", ex: "log(exp(5))  % → 5" },
  log2: { sig: "log2(x)", desc: "Base-2 logarithm", cat: "Math", ex: "log2(8)  % → 3" },
  log10: { sig: "log10(x)", desc: "Base-10 logarithm", cat: "Math", ex: "log10(100)  % → 2" },
  floor: { sig: "floor(x)", desc: "Round toward negative infinity", cat: "Math", ex: "floor(3.7)  % → 3" },
  ceil: { sig: "ceil(x)", desc: "Round toward positive infinity", cat: "Math", ex: "ceil(-2.3)  % → -2" },
  round: { sig: "round(x)", desc: "Round to nearest integer", cat: "Math", ex: "round(3.5)  % → 4" },
  mod: { sig: "mod(a, b)", desc: "Modulus after division", cat: "Math", ex: "mod(17, 5)  % → 2" },
  rem: { sig: "rem(a, b)", desc: "Remainder after division", cat: "Math", ex: "rem(17, 5)  % → 2" },
  pow: { sig: "pow(x, y)", desc: "x raised to power y", cat: "Math", ex: "pow(2, 10)  % → 1024" },
  sign: { sig: "sign(x)", desc: "Sign of x (-1, 0, or 1)", cat: "Math", ex: "sign(-42)  % → -1" },
  // Matrix
  zeros: { sig: "zeros(n) / zeros(m,n)", desc: "Create matrix of zeros", cat: "Matrix", ex: "zeros(2,3)" },
  ones: { sig: "ones(n) / ones(m,n)", desc: "Create matrix of ones", cat: "Matrix", ex: "ones(1,4)" },
  eye: { sig: "eye(n)", desc: "Identity matrix", cat: "Matrix", ex: "eye(3)" },
  rand: { sig: "rand(n) / rand(m,n)", desc: "Uniform random matrix [0,1)", cat: "Matrix", ex: "rand(2,3)" },
  randn: { sig: "randn(n) / randn(m,n)", desc: "Normal random matrix", cat: "Matrix", ex: "randn(3)" },
  linspace: { sig: "linspace(a, b, n)", desc: "n linearly spaced points from a to b", cat: "Matrix", ex: "linspace(0, 1, 5)" },
  reshape: { sig: "reshape(A, m, n)", desc: "Reshape matrix to m×n", cat: "Matrix", ex: "reshape(1:6, 2, 3)" },
  size: { sig: "size(A)", desc: "Dimensions of matrix A", cat: "Matrix", ex: "size([1 2; 3 4])" },
  length: { sig: "length(A)", desc: "Length of largest dimension", cat: "Matrix", ex: "length([1 2 3 4])" },
  numel: { sig: "numel(A)", desc: "Total number of elements", cat: "Matrix", ex: "numel([1 2; 3 4])" },
  // Aggregation
  sum: { sig: "sum(A)", desc: "Sum of elements", cat: "Aggregation", ex: "sum([1 2 3 4 5])  % → 15" },
  prod: { sig: "prod(A)", desc: "Product of elements", cat: "Aggregation", ex: "prod([1 2 3 4])  % → 24" },
  mean: { sig: "mean(A)", desc: "Average of elements", cat: "Aggregation", ex: "mean([2 4 6])  % → 4" },
  min: { sig: "min(A)", desc: "Minimum element", cat: "Aggregation", ex: "min([5 1 3])  % → 1" },
  max: { sig: "max(A)", desc: "Maximum element", cat: "Aggregation", ex: "max([5 1 3])  % → 5" },
  cumsum: { sig: "cumsum(A)", desc: "Cumulative sum", cat: "Aggregation", ex: "cumsum([1 2 3])  % → [1 3 6]" },
  sort: { sig: "sort(A)", desc: "Sort in ascending order", cat: "Aggregation", ex: "sort([3 1 2])  % → [1 2 3]" },
  // String
  upper: { sig: "upper(s)", desc: "Convert string to uppercase", cat: "String", ex: "upper('hello')  % → 'HELLO'" },
  lower: { sig: "lower(s)", desc: "Convert string to lowercase", cat: "String", ex: "lower('MATLAB')  % → 'matlab'" },
  strcmp: { sig: "strcmp(s1, s2)", desc: "Compare strings (case-sensitive)", cat: "String", ex: "strcmp('a', 'a')  % → 1" },
  strcmpi: { sig: "strcmpi(s1, s2)", desc: "Compare strings (case-insensitive)", cat: "String", ex: "strcmpi('A', 'a')  % → 1" },
  strcat: { sig: "strcat(s1, s2, ...)", desc: "Concatenate strings", cat: "String", ex: "strcat('he','llo')" },
  strsplit: { sig: "strsplit(s, delim)", desc: "Split string by delimiter", cat: "String", ex: "strsplit('a,b,c', ',')" },
  num2str: { sig: "num2str(x)", desc: "Convert number to string", cat: "String", ex: "num2str(42)" },
  sprintf: { sig: "sprintf(fmt, ...)", desc: "Formatted string output", cat: "String", ex: "sprintf('x = %d', 42)" },
  // I/O
  disp: { sig: "disp(x)", desc: "Display value of x", cat: "I/O", ex: "disp('Hello')" },
  fprintf: { sig: "fprintf(fmt, ...)", desc: "Formatted output to console", cat: "I/O", ex: "fprintf('x = %d\\n', 42)" },
  // Complex
  real: { sig: "real(z)", desc: "Real part of complex number", cat: "Complex", ex: "real(3+4i)  % → 3" },
  imag: { sig: "imag(z)", desc: "Imaginary part of complex number", cat: "Complex", ex: "imag(3+4i)  % → 4" },
  conj: { sig: "conj(z)", desc: "Complex conjugate", cat: "Complex", ex: "conj(3+4i)  % → 3-4i" },
  // Workspace
  clear: { sig: "clear", desc: "Clear all variables from workspace", cat: "Workspace", ex: "clear" },
  clc: { sig: "clc", desc: "Clear terminal screen", cat: "Workspace", ex: "clc" },
  who: { sig: "who", desc: "List variable names", cat: "Workspace", ex: "who" },
  whos: { sig: "whos", desc: "List variables with details", cat: "Workspace", ex: "whos" },
  // Type checks
  isempty: { sig: "isempty(A)", desc: "True if A is empty", cat: "Type", ex: "isempty([])  % → 1" },
  isnumeric: { sig: "isnumeric(x)", desc: "True if x is numeric", cat: "Type", ex: "isnumeric(42)  % → 1" },
  ischar: { sig: "ischar(x)", desc: "True if x is a string", cat: "Type", ex: "ischar('hi')  % → 1" },
  // Plotting (simulated)
  plot: { sig: "plot(x, y) / plot(y)", desc: "2D line plot", cat: "Plotting", ex: "plot(sin(linspace(0,2*pi,50)))" },
  bar: { sig: "bar(x, y) / bar(y)", desc: "Bar chart", cat: "Plotting", ex: "bar([1 4 2 5 3])" },
  scatter: { sig: "scatter(x, y)", desc: "Scatter plot", cat: "Plotting", ex: "scatter(rand(1,20), rand(1,20))" },
  hist: { sig: "hist(data, bins)", desc: "Histogram", cat: "Plotting", ex: "hist(randn(1,1000), 20)" },
  title: { sig: "title('text')", desc: "Set plot title", cat: "Plotting", ex: "title('My Plot')" },
  xlabel: { sig: "xlabel('text')", desc: "Set x-axis label", cat: "Plotting", ex: "xlabel('Time')" },
  ylabel: { sig: "ylabel('text')", desc: "Set y-axis label", cat: "Plotting", ex: "ylabel('Value')" },
};

// ── Cheat Sheet Data ──
const CHEAT_SHEET = [
  {
    title: "Operators",
    items: [
      { code: "+ - * /", desc: "Arithmetic" },
      { code: ".* ./ .^", desc: "Element-wise ops" },
      { code: "^", desc: "Power / matrix power" },
      { code: "== ~= < > <= >=", desc: "Comparison" },
      { code: "& | ~ &&  ||", desc: "Logical" },
      { code: ":", desc: "Range / slicing" },
      { code: "'", desc: "Transpose" },
    ],
  },
  {
    title: "Matrix Creation",
    items: [
      { code: "[1 2 3; 4 5 6]", desc: "Manual matrix" },
      { code: "zeros(m,n)", desc: "Zero matrix" },
      { code: "ones(m,n)", desc: "Ones matrix" },
      { code: "eye(n)", desc: "Identity matrix" },
      { code: "rand(m,n)", desc: "Random [0,1)" },
      { code: "linspace(a,b,n)", desc: "Linear spacing" },
      { code: "a:step:b", desc: "Range with step" },
    ],
  },
  {
    title: "Indexing",
    items: [
      { code: "A(i,j)", desc: "Element access" },
      { code: "A(i,:)", desc: "Entire row i" },
      { code: "A(:,j)", desc: "Entire column j" },
      { code: "A(2:4, 1:3)", desc: "Submatrix" },
      { code: "A(end)", desc: "Last element" },
    ],
  },
  {
    title: "Control Flow",
    items: [
      { code: "if / elseif / else / end", desc: "Conditional" },
      { code: "for i = 1:n ... end", desc: "For loop" },
      { code: "while cond ... end", desc: "While loop" },
      { code: "switch / case / end", desc: "Switch" },
      { code: "try / catch / end", desc: "Error handling" },
      { code: "break / continue", desc: "Loop control" },
    ],
  },
  {
    title: "Functions",
    items: [
      { code: "function y = f(x)", desc: "Single output" },
      { code: "function [a,b] = f(x)", desc: "Multiple outputs" },
      { code: "return", desc: "Early return" },
    ],
  },
  {
    title: "Keyboard Shortcuts",
    items: [
      { code: "Enter", desc: "Execute command" },
      { code: "Shift+Enter", desc: "New line" },
      { code: "Tab", desc: "Autocomplete" },
      { code: "↑ / ↓", desc: "History navigation" },
      { code: "Ctrl+L", desc: "Clear screen" },
      { code: "Ctrl+C", desc: "Cancel input" },
    ],
  },
];

// ── Examples ──
const EXAMPLES = [
  {
    category: "Basics", icon: "📐",
    items: [
      { title: "Arithmetic", description: "Basic math operations", code: "disp(2 + 3 * 4 - 1)\ndisp((2 + 3) * (4 - 1))\ndisp(2 ^ 3 ^ 2)\ndisp(mod(17, 5))" },
      { title: "Variables", description: "Assign and use variables", code: "x = 42;\ny = 3.14;\nresult = x * y;\ndisp(result)" },
      { title: "Math functions", description: "sqrt, abs, log, exp, trig", code: "disp(sqrt(144))\ndisp(abs(-7))\ndisp(log(exp(5)))\ndisp(floor(3.7))\ndisp(ceil(-2.3))" },
      { title: "Strings", description: "String operations", code: "disp(upper('hello'))\ndisp(lower('MATLAB'))\ndisp(length('OpenAI'))\ndisp(strcmp('test', 'test'))" },
    ],
  },
  {
    category: "Matrices", icon: "🔢",
    items: [
      { title: "Create & index", description: "Matrix creation and element access", code: "A = [1 2 3; 4 5 6; 7 8 9];\ndisp(A)\ndisp(A(2, 3))" },
      { title: "Matrix arithmetic", description: "Addition and element-wise multiply", code: "A = [1 2 3; 4 5 6; 7 8 9];\nB = [9 8 7; 6 5 4; 3 2 1];\ndisp(A + B)\ndisp(A .* B)" },
      { title: "Special matrices", description: "eye, zeros, ones, linspace", code: "disp(eye(3))\ndisp(zeros(2))\ndisp(ones(1, 4))\ndisp(linspace(0, 1, 5))" },
      { title: "Vector operations", description: "Ranges, sum, min, max", code: "v = 1:10;\ndisp(v)\ndisp(sum(v))\ndisp(min(v))\ndisp(max(v))" },
    ],
  },
  {
    category: "Control Flow", icon: "🔄",
    items: [
      { title: "For loop", description: "Sum of 1 to 100", code: "total = 0;\nfor i = 1:100\n    total = total + i;\nend\ndisp(total)" },
      { title: "While loop", description: "First power of 2 >= 1000", code: "n = 1;\nwhile n < 1000\n    n = n * 2;\nend\ndisp(n)" },
      { title: "If / elseif / else", description: "Conditional branching", code: "val = 42;\nif val > 100\n    disp('big')\nelseif val > 10\n    disp('medium')\nelse\n    disp('small')\nend" },
      { title: "Nested loops", description: "Multiplication table", code: "for i = 1:5\n    row = '';\n    for j = 1:5\n        row = [row, num2str(i*j), ' '];\n    end\n    disp(row)\nend" },
    ],
  },
  {
    category: "Functions", icon: "⚡",
    items: [
      { title: "Factorial", description: "Recursive factorial function", code: "function y = factorial(n)\n    if n <= 1\n        y = 1;\n    else\n        y = n * factorial(n - 1);\n    end\nend\n\ndisp(factorial(10))" },
      { title: "Fibonacci", description: "Recursive Fibonacci sequence", code: "function r = fib(n)\n    if n <= 1\n        r = n;\n    else\n        r = fib(n-1) + fib(n-2);\n    end\nend\n\nresult = [];\nfor k = 0:9\n    result = [result, fib(k)];\nend\ndisp(result)" },
      { title: "Multiple returns", description: "Function returning min and max", code: "function [mn, mx] = minmax(v)\n    mn = v(1); mx = v(1);\n    for i = 2:length(v)\n        if v(i) < mn\n            mn = v(i);\n        end\n        if v(i) > mx\n            mx = v(i);\n        end\n    end\nend\n\n[lo, hi] = minmax([5 3 9 1 7]);\ndisp(lo)\ndisp(hi)" },
    ],
  },
  {
    category: "Plotting", icon: "📊",
    items: [
      { title: "Sine wave", description: "Basic line plot", code: "x = linspace(0, 2*pi, 100);\ny = sin(x);\nplot(x, y)\ntitle('Sine Wave')\nxlabel('x')\nylabel('sin(x)')" },
      { title: "Multiple curves", description: "Plot sin & cos together", code: "x = linspace(0, 2*pi, 100);\nplot(x, sin(x))\nplot(x, cos(x))\ntitle('Trig Functions')" },
      { title: "Bar chart", description: "Simple bar chart", code: "bar([4 7 2 9 5 3 8])\ntitle('Weekly Data')" },
      { title: "Scatter plot", description: "Random scatter", code: "x = rand(1, 30);\ny = rand(1, 30);\nscatter(x, y)\ntitle('Random Points')" },
    ],
  },
  {
    category: "Structures", icon: "🏗️",
    items: [
      { title: "Basic struct", description: "Create and access struct fields", code: "person.name  = 'Alice';\nperson.age   = 30;\nperson.score = [95 87 92];\ndisp(person.name)\ndisp(person.age)" },
      { title: "Nested structs", description: "Structs inside structs", code: "car.make = 'Toyota';\ncar.year = 2024;\ncar.engine.horsepower = 203;\ncar.engine.type = 'hybrid';\ndisp(car.make)\ndisp(car.engine.horsepower)" },
    ],
  },
  {
    category: "Algorithms", icon: "🧮",
    items: [
      { title: "Bubble Sort", description: "Sort with swap counter", code: "function result = bubbleSort(arr)\n    n = length(arr);\n    swaps = 0;\n    for i = 1:n-1\n        for j = 1:n-i\n            if arr(j) > arr(j+1)\n                temp = arr(j);\n                arr(j) = arr(j+1);\n                arr(j+1) = temp;\n                swaps = swaps + 1;\n            end\n        end\n    end\n    result.sorted = arr;\n    result.swaps  = swaps;\nend\n\ninfo = bubbleSort([64 34 25 12 22 11 90]);\ndisp(info.sorted)" },
      { title: "Sieve of Eratosthenes", description: "Find all primes up to N", code: "function primes = sieve(limit)\n    is_prime = ones(1, limit);\n    is_prime(1) = 0;\n    for i = 2:floor(sqrt(limit))\n        if is_prime(i)\n            for j = i*i:i:limit\n                is_prime(j) = 0;\n            end\n        end\n    end\n    count = 0;\n    for i = 1:limit\n        if is_prime(i)\n            count = count + 1;\n        end\n    end\n    primes = zeros(1, count);\n    idx = 1;\n    for i = 1:limit\n        if is_prime(i)\n            primes(idx) = i;\n            idx = idx + 1;\n        end\n    end\nend\n\ndisp(sieve(30))" },
      { title: "FizzBuzz", description: "Classic challenge", code: "for i = 1:20\n    if mod(i, 15) == 0\n        disp('FizzBuzz')\n    elseif mod(i, 3) == 0\n        disp('Fizz')\n    elseif mod(i, 5) == 0\n        disp('Buzz')\n    else\n        disp(i)\n    end\nend" },
      { title: "Newton sqrt", description: "Square root via Newton method", code: "function r = newton_sqrt(x)\n    r = x;\n    for i = 1:20\n        r = (r + x/r) / 2;\n    end\nend\n\ndisp(newton_sqrt(2))\ndisp(newton_sqrt(144))" },
    ],
  },
  {
    category: "Advanced", icon: "🚀",
    items: [
      { title: "Matrix operations", description: "Transpose, trace, reshape", code: "A = [1 2 3; 4 5 6];\ndisp(A')\ndisp(reshape(A, 3, 2))\ndisp(size(A))" },
      { title: "Complex numbers", description: "Complex arithmetic", code: "z1 = 3 + 4i;\nz2 = 1 - 2i;\ndisp(abs(z1))\ndisp(real(z1))\ndisp(imag(z1))" },
      { title: "String processing", description: "String manipulation", code: "s = 'Hello, World!';\ndisp(upper(s))\ndisp(lower(s))\ndisp(length(s))" },
      { title: "Statistical analysis", description: "Mean, variance, std via loops", code: "data = [4 8 15 16 23 42];\nn = length(data);\n\nm = sum(data) / n;\ndisp(m)\n\nv = 0;\nfor i = 1:n\n    v = v + (data(i) - m)^2;\nend\nv = v / (n - 1);\ndisp(v)\ndisp(sqrt(v))" },
    ],
  },
];

// ── Simple MATLAB-like interpreter (fallback / demo) ──
function createInterpreter() {
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

// ── Color palette ──
const C = {
  bg0: "#0f0f17", bg1: "#161622", bg2: "#1c1c2e", bg3: "#262640",
  bg4: "#2e2e4a", border: "#363658", borderHi: "#4a4a7a",
  text: "#d4d4f0", textDim: "#8888b0", textMuted: "#5a5a80",
  accent: "#7c6ff0", accentDim: "#5a50b0",
  green: "#6ee7a0", greenDim: "#3a8a5a",
  red: "#f07070", orange: "#f0a060", cyan: "#60d0f0", yellow: "#e8d060",
  pink: "#e070c0",
};

// ── Plot Component ──
function PlotPanel({ data, onClose }) {
  const svgRef = useRef(null);
  const containerRef = useRef(null);

  useEffect(() => {
    if (!data || !svgRef.current || !containerRef.current) return;
    const svg = d3.select(svgRef.current);
    svg.selectAll("*").remove();

    const cw = containerRef.current.clientWidth;
    const width = Math.min(cw - 8, 600);
    const height = 300;
    const margin = { top: 30, right: 20, bottom: 40, left: 50 };
    const iw = width - margin.left - margin.right;
    const ih = height - margin.top - margin.bottom;

    svg.attr("width", width).attr("height", height);
    const g = svg.append("g").attr("transform", `translate(${margin.left},${margin.top})`);

    const colors = [C.accent, C.cyan, C.green, C.orange, C.pink, C.yellow];

    const allX = data.datasets.flatMap((d) => d.x);
    const allY = data.datasets.flatMap((d) => d.y);
    const xScale = d3.scaleLinear().domain([Math.min(...allX), Math.max(...allX)]).range([0, iw]).nice();
    const yScale = d3.scaleLinear().domain([Math.min(...allY) * 0.95, Math.max(...allY) * 1.05]).range([ih, 0]).nice();

    // Grid
    g.append("g").attr("class", "grid")
      .selectAll("line").data(yScale.ticks(5)).enter()
      .append("line")
      .attr("x1", 0).attr("x2", iw)
      .attr("y1", d => yScale(d)).attr("y2", d => yScale(d))
      .attr("stroke", C.border).attr("stroke-dasharray", "2,4");

    // Axes
    g.append("g").attr("transform", `translate(0,${ih})`)
      .call(d3.axisBottom(xScale).ticks(6))
      .selectAll("text,line,path").attr("fill", C.textDim).attr("stroke", C.textDim);
    g.append("g")
      .call(d3.axisLeft(yScale).ticks(5))
      .selectAll("text,line,path").attr("fill", C.textDim).attr("stroke", C.textDim);

    // Data
    data.datasets.forEach((ds, idx) => {
      const color = colors[idx % colors.length];
      if (data.config.type === "line") {
        const lineGen = d3.line()
          .x((_, i) => xScale(ds.x[i]))
          .y((_, i) => yScale(ds.y[i]))
          .curve(d3.curveMonotoneX);
        g.append("path")
          .datum(ds.y)
          .attr("d", lineGen)
          .attr("fill", "none")
          .attr("stroke", color)
          .attr("stroke-width", 2);
      } else if (data.config.type === "scatter") {
        g.selectAll(`.dot-${idx}`)
          .data(ds.x.map((x, i) => ({ x, y: ds.y[i] })))
          .enter().append("circle")
          .attr("cx", d => xScale(d.x))
          .attr("cy", d => yScale(d.y))
          .attr("r", 4)
          .attr("fill", color)
          .attr("opacity", 0.8);
      } else if (data.config.type === "bar") {
        const bw = Math.max(2, iw / ds.x.length * 0.7);
        g.selectAll(`.bar-${idx}`)
          .data(ds.x.map((x, i) => ({ x, y: ds.y[i] })))
          .enter().append("rect")
          .attr("x", d => xScale(d.x) - bw / 2)
          .attr("y", d => yScale(d.y))
          .attr("width", bw)
          .attr("height", d => ih - yScale(d.y))
          .attr("fill", color)
          .attr("opacity", 0.85)
          .attr("rx", 2);
      }
    });

    // Title
    if (data.config.title) {
      svg.append("text")
        .attr("x", width / 2).attr("y", 18)
        .attr("text-anchor", "middle")
        .attr("fill", C.text).attr("font-size", 13).attr("font-weight", 600)
        .text(data.config.title);
    }
    if (data.config.xlabel) {
      svg.append("text")
        .attr("x", width / 2).attr("y", height - 4)
        .attr("text-anchor", "middle")
        .attr("fill", C.textDim).attr("font-size", 11)
        .text(data.config.xlabel);
    }
    if (data.config.ylabel) {
      svg.append("text")
        .attr("transform", `translate(14,${height/2}) rotate(-90)`)
        .attr("text-anchor", "middle")
        .attr("fill", C.textDim).attr("font-size", 11)
        .text(data.config.ylabel);
    }
  }, [data]);

  if (!data) return null;

  return (
    <div ref={containerRef} style={{
      background: C.bg1, border: `1px solid ${C.border}`, borderRadius: 8,
      margin: "6px 0", padding: 8, position: "relative",
    }}>
      <button onClick={onClose} style={{
        position: "absolute", top: 6, right: 8, background: "none", border: "none",
        color: C.textMuted, cursor: "pointer", fontSize: 16, lineHeight: 1,
      }}>×</button>
      <svg ref={svgRef} style={{ display: "block", margin: "0 auto" }} />
    </div>
  );
}

// ── Tabs / File Manager ──
function TabBar({ tabs, activeTab, onSelect, onClose, onNew, onRename }) {
  const [editingId, setEditingId] = useState(null);
  const [editName, setEditName] = useState("");

  return (
    <div style={{
      display: "flex", alignItems: "center", gap: 2, padding: "4px 8px",
      background: C.bg0, borderBottom: `1px solid ${C.border}`, overflowX: "auto",
      minHeight: 34, flexShrink: 0,
    }}>
      {tabs.map((tab) => (
        <div key={tab.id} onClick={() => onSelect(tab.id)}
          style={{
            display: "flex", alignItems: "center", gap: 4,
            padding: "4px 10px", borderRadius: 6, cursor: "pointer", fontSize: 12,
            background: tab.id === activeTab ? C.bg3 : "transparent",
            color: tab.id === activeTab ? C.text : C.textMuted,
            border: `1px solid ${tab.id === activeTab ? C.borderHi : "transparent"}`,
            whiteSpace: "nowrap", transition: "all 0.15s",
          }}>
          {editingId === tab.id ? (
            <input value={editName} autoFocus
              onChange={e => setEditName(e.target.value)}
              onBlur={() => { onRename(tab.id, editName); setEditingId(null); }}
              onKeyDown={e => { if (e.key === "Enter") { onRename(tab.id, editName); setEditingId(null); } }}
              style={{
                background: "transparent", border: "none", color: C.text,
                fontSize: 12, width: 80, outline: "none",
                fontFamily: "'IBM Plex Mono', 'JetBrains Mono', monospace",
              }}
              onClick={e => e.stopPropagation()}
            />
          ) : (
            <span onDoubleClick={(e) => { e.stopPropagation(); setEditingId(tab.id); setEditName(tab.name); }}>
              {tab.name}{tab.modified ? " •" : ""}
            </span>
          )}
          {tabs.length > 1 && (
            <span onClick={(e) => { e.stopPropagation(); onClose(tab.id); }}
              style={{ color: C.textMuted, fontSize: 14, lineHeight: 1, marginLeft: 2, opacity: 0.6 }}>×</span>
          )}
        </div>
      ))}
      <button onClick={onNew} style={{
        background: "none", border: `1px dashed ${C.border}`, borderRadius: 6,
        color: C.textMuted, fontSize: 14, padding: "2px 8px", cursor: "pointer",
        lineHeight: 1, marginLeft: 4,
      }}>+</button>
    </div>
  );
}

// ── Variable Inspector ──
function VarInspector({ variables, visible, onClose }) {
  if (!visible) return null;
  const entries = Object.entries(variables);

  const getType = (v) => {
    if (Array.isArray(v)) {
      if (v.length && Array.isArray(v[0])) return "matrix";
      return "vector";
    }
    if (typeof v === "string") return "char";
    if (typeof v === "object" && v !== null) return "struct";
    return "double";
  };

  const getSize = (v) => {
    if (Array.isArray(v)) {
      if (v.length && Array.isArray(v[0])) return `${v.length}×${v[0].length}`;
      return `1×${v.length}`;
    }
    if (typeof v === "string") return `1×${v.length}`;
    if (typeof v === "object" && v !== null) return `1×${Object.keys(v).length}`;
    return "1×1";
  };

  const getPreview = (v) => {
    if (Array.isArray(v)) {
      const flat = v.flat();
      if (flat.length <= 8) return `[${flat.map(x => typeof x === "number" ? (Number.isInteger(x) ? x : x.toFixed(3)) : x).join(", ")}]`;
      return `[${flat.slice(0, 5).map(x => typeof x === "number" ? (Number.isInteger(x) ? x : x.toFixed(3)) : x).join(", ")}, ...]`;
    }
    if (typeof v === "string") return `'${v}'`;
    if (typeof v === "object" && v !== null) {
      const keys = Object.keys(v);
      return `{${keys.join(", ")}}`;
    }
    if (typeof v === "number") return Number.isInteger(v) ? String(v) : v.toFixed(6);
    return String(v);
  };

  const typeColors = { double: C.cyan, vector: C.green, matrix: C.accent, char: C.yellow, struct: C.orange };

  return (
    <div style={{
      width: 280, minWidth: 280, flexShrink: 0,
      background: C.bg1, borderLeft: `1px solid ${C.border}`,
      display: "flex", flexDirection: "column", overflow: "hidden",
    }}>
      <div style={{
        display: "flex", justifyContent: "space-between", alignItems: "center",
        padding: "10px 14px", borderBottom: `1px solid ${C.border}`, flexShrink: 0,
      }}>
        <span style={{ fontSize: 13, fontWeight: 600, color: C.text }}>Workspace</span>
        <button onClick={onClose} style={{
          background: "none", border: "none", color: C.textMuted, cursor: "pointer",
          fontSize: 18, lineHeight: 1,
        }}>×</button>
      </div>
      <div style={{ flex: 1, overflowY: "auto", padding: 8 }}>
        {!entries.length ? (
          <div style={{ color: C.textMuted, fontSize: 12, padding: 12, textAlign: "center" }}>
            No variables in workspace
          </div>
        ) : entries.map(([name, val]) => {
          const type = getType(val);
          return (
            <div key={name} style={{
              padding: "8px 10px", marginBottom: 4, borderRadius: 6,
              background: C.bg2, border: `1px solid ${C.border}`,
            }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 4 }}>
                <span style={{ fontSize: 13, fontWeight: 600, color: C.text }}>{name}</span>
                <div style={{ display: "flex", gap: 6, alignItems: "center" }}>
                  <span style={{ fontSize: 10, color: C.textMuted }}>{getSize(val)}</span>
                  <span style={{
                    fontSize: 10, padding: "1px 6px", borderRadius: 4,
                    background: `${typeColors[type] || C.textDim}22`,
                    color: typeColors[type] || C.textDim,
                  }}>{type}</span>
                </div>
              </div>
              <div style={{
                fontSize: 11, color: C.textDim, fontFamily: "'IBM Plex Mono', monospace",
                whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis",
              }}>{getPreview(val)}</div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

// ── Cheat Sheet Panel ──
function CheatSheet({ visible, onClose }) {
  if (!visible) return null;
  return (
    <div style={{
      background: C.bg1, borderBottom: `1px solid ${C.border}`, flexShrink: 0,
      maxHeight: "45vh", overflowY: "auto",
    }}>
      <div style={{
        display: "flex", justifyContent: "space-between", alignItems: "center",
        padding: "10px 16px", position: "sticky", top: 0, background: C.bg1, zIndex: 1,
        borderBottom: `1px solid ${C.border}`,
      }}>
        <span style={{ fontSize: 13, fontWeight: 600, color: C.text }}>Quick Reference</span>
        <button onClick={onClose} style={{
          background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 18,
        }}>×</button>
      </div>
      <div style={{
        padding: 12,
        display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(220px, 1fr))", gap: 12,
        alignContent: "start",
      }}>
        {CHEAT_SHEET.map((section) => (
          <div key={section.title} style={{
            background: C.bg2, borderRadius: 8, padding: 12,
            border: `1px solid ${C.border}`,
          }}>
            <div style={{ fontSize: 12, fontWeight: 600, color: C.accent, marginBottom: 8, textTransform: "uppercase", letterSpacing: 0.5 }}>
              {section.title}
            </div>
            {section.items.map((item, i) => (
              <div key={i} style={{ display: "flex", justifyContent: "space-between", gap: 8, marginBottom: 4 }}>
                <code style={{ fontSize: 11, color: C.green, whiteSpace: "nowrap" }}>{item.code}</code>
                <span style={{ fontSize: 10, color: C.textMuted, textAlign: "right" }}>{item.desc}</span>
              </div>
            ))}
          </div>
        ))}
      </div>
    </div>
  );
}

// ── Help Panel ──
function HelpPanel({ topic, onClose }) {
  if (!topic) return null;
  const info = HELP_DB[topic];
  if (!info) return null;

  return (
    <div style={{
      background: C.bg2, border: `1px solid ${C.borderHi}`, borderRadius: 8,
      padding: "10px 14px", margin: "6px 0", position: "relative",
    }}>
      <button onClick={onClose} style={{
        position: "absolute", top: 6, right: 8, background: "none", border: "none",
        color: C.textMuted, cursor: "pointer", fontSize: 16,
      }}>×</button>
      <div style={{ fontSize: 14, fontWeight: 700, color: C.accent, marginBottom: 4 }}>{info.sig}</div>
      <div style={{ fontSize: 12, color: C.text, marginBottom: 4 }}>{info.desc}</div>
      <div style={{ fontSize: 11, color: C.textMuted }}>Category: {info.cat}</div>
      <div style={{ fontSize: 12, color: C.green, marginTop: 4, fontFamily: "'IBM Plex Mono', monospace" }}>{info.ex}</div>
    </div>
  );
}

// ── Examples Panel ──
function ExamplesPanel({ visible, onClose, onRun }) {
  if (!visible) return null;
  return (
    <div style={{
      background: C.bg1, borderBottom: `1px solid ${C.border}`, flexShrink: 0,
      maxHeight: "50vh", overflowY: "auto",
    }}>
      <div style={{
        display: "flex", justifyContent: "space-between", alignItems: "center",
        padding: "10px 16px", position: "sticky", top: 0, background: C.bg1, zIndex: 1,
        borderBottom: `1px solid ${C.border}`,
      }}>
        <span style={{ fontSize: 13, fontWeight: 600, color: C.text }}>Examples</span>
        <button onClick={onClose} style={{
          background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 18,
        }}>×</button>
      </div>
      <div style={{ padding: "8px 16px 16px" }}>
        {EXAMPLES.map((cat) => (
          <div key={cat.category} style={{ marginBottom: 14 }}>
            <div style={{
              fontSize: 11, fontWeight: 600, color: C.accent, textTransform: "uppercase",
              letterSpacing: 0.5, marginBottom: 6,
            }}>{cat.icon} {cat.category}</div>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(240px, 1fr))", gap: 6 }}>
              {cat.items.map((item) => (
                <div key={item.title} onClick={() => onRun(item)}
                  style={{
                    background: C.bg2, border: `1px solid ${C.border}`, borderRadius: 8,
                    padding: "10px 12px", cursor: "pointer", transition: "all 0.15s",
                  }}
                  onMouseEnter={e => { e.currentTarget.style.borderColor = C.accent; e.currentTarget.style.background = C.bg3; }}
                  onMouseLeave={e => { e.currentTarget.style.borderColor = C.border; e.currentTarget.style.background = C.bg2; }}
                >
                  <div style={{ fontSize: 12, fontWeight: 600, color: C.text }}>{item.title}</div>
                  <div style={{ fontSize: 10, color: C.textMuted, marginBottom: 4 }}>{item.description}</div>
                  <pre style={{
                    fontSize: 10, color: C.textDim, background: C.bg0, borderRadius: 4,
                    padding: "4px 6px", margin: 0, overflow: "hidden", maxHeight: "3em",
                    whiteSpace: "pre", fontFamily: "'IBM Plex Mono', monospace",
                  }}>
                    {item.code.split("\n").slice(0, 3).join("\n")}{item.code.split("\n").length > 3 ? "\n..." : ""}
                  </pre>
                </div>
              ))}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

// ── Git Repo Browser ──
function GitRepoBrowser({ visible, onClose, onOpenFile, onRunFile }) {
  const [repoUrl, setRepoUrl] = useState("");
  const [tree, setTree] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [expanded, setExpanded] = useState({});
  const [previewFile, setPreviewFile] = useState(null);
  const [previewContent, setPreviewContent] = useState("");
  const [previewLoading, setPreviewLoading] = useState(false);
  const [branch, setBranch] = useState("main");
  const [branches, setBranches] = useState([]);
  const [repoInfo, setRepoInfo] = useState(null);

  const parseRepoUrl = (url) => {
    // Supports: https://github.com/owner/repo, owner/repo, github.com/owner/repo
    const cleaned = url.trim().replace(/\/+$/, "").replace(/\.git$/, "");
    let m = cleaned.match(/github\.com\/([^/]+)\/([^/]+)/);
    if (m) return { owner: m[1], repo: m[2] };
    m = cleaned.match(/^([^/\s]+)\/([^/\s]+)$/);
    if (m) return { owner: m[1], repo: m[2] };
    return null;
  };

  const fetchRepo = async () => {
    const parsed = parseRepoUrl(repoUrl);
    if (!parsed) { setError("Invalid format. Use: owner/repo or https://github.com/owner/repo"); return; }
    setLoading(true); setError(""); setTree(null); setPreviewFile(null);

    try {
      // Fetch repo info
      const infoRes = await fetch(`https://api.github.com/repos/${parsed.owner}/${parsed.repo}`);
      if (!infoRes.ok) throw new Error(infoRes.status === 404 ? "Repository not found" : `GitHub API error: ${infoRes.status}`);
      const info = await infoRes.json();
      setRepoInfo(info);

      // Fetch branches
      const brRes = await fetch(`https://api.github.com/repos/${parsed.owner}/${parsed.repo}/branches?per_page=20`);
      if (brRes.ok) {
        const brs = await brRes.json();
        setBranches(brs.map(b => b.name));
        if (!brs.find(b => b.name === branch)) {
          setBranch(info.default_branch || "main");
        }
      }

      // Fetch tree recursively
      const treeRes = await fetch(
        `https://api.github.com/repos/${parsed.owner}/${parsed.repo}/git/trees/${info.default_branch || "main"}?recursive=1`
      );
      if (!treeRes.ok) throw new Error("Failed to fetch file tree");
      const treeData = await treeRes.json();
      setTree(treeData.tree || []);
      setBranch(info.default_branch || "main");
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  const fetchBranch = async (branchName) => {
    const parsed = parseRepoUrl(repoUrl);
    if (!parsed) return;
    setLoading(true); setError(""); setBranch(branchName); setPreviewFile(null);
    try {
      const treeRes = await fetch(
        `https://api.github.com/repos/${parsed.owner}/${parsed.repo}/git/trees/${branchName}?recursive=1`
      );
      if (!treeRes.ok) throw new Error("Failed to fetch branch tree");
      const treeData = await treeRes.json();
      setTree(treeData.tree || []);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  const fetchFileContent = async (path) => {
    const parsed = parseRepoUrl(repoUrl);
    if (!parsed) return;
    setPreviewLoading(true); setPreviewFile(path);
    try {
      const res = await fetch(
        `https://api.github.com/repos/${parsed.owner}/${parsed.repo}/contents/${path}?ref=${branch}`
      );
      if (!res.ok) throw new Error("Failed to fetch file");
      const data = await res.json();
      if (data.encoding === "base64") {
        setPreviewContent(atob(data.content));
      } else {
        setPreviewContent(data.content || "");
      }
    } catch (err) {
      setPreviewContent(`Error loading file: ${err.message}`);
    } finally {
      setPreviewLoading(false);
    }
  };

  // Build nested tree structure from flat list
  const buildTree = (items) => {
    if (!items) return [];
    const root = { children: {} };
    for (const item of items) {
      const parts = item.path.split("/");
      let node = root;
      for (let i = 0; i < parts.length; i++) {
        const part = parts[i];
        if (!node.children[part]) {
          node.children[part] = {
            name: part,
            path: parts.slice(0, i + 1).join("/"),
            type: i === parts.length - 1 ? item.type : "tree",
            size: item.size,
            children: {},
          };
        }
        node = node.children[part];
      }
    }
    return root.children;
  };

  const toggleExpand = (path) => {
    setExpanded(prev => ({ ...prev, [path]: !prev[path] }));
  };

  const getFileIcon = (name) => {
    if (name.endsWith(".m")) return "📄";
    if (name.endsWith(".md") || name.endsWith(".txt")) return "📝";
    if (name.endsWith(".json")) return "📋";
    if (name.endsWith(".cpp") || name.endsWith(".hpp") || name.endsWith(".h") || name.endsWith(".c")) return "⚙️";
    if (name.endsWith(".py")) return "🐍";
    if (name.endsWith(".js") || name.endsWith(".ts") || name.endsWith(".jsx")) return "🟨";
    if (name.endsWith(".css") || name.endsWith(".html")) return "🌐";
    if (name.endsWith(".yml") || name.endsWith(".yaml") || name.endsWith(".toml")) return "⚙️";
    if (name.endsWith(".sh") || name.endsWith(".bash")) return "💻";
    if (name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".svg")) return "🖼️";
    if (name === "Makefile" || name === "CMakeLists.txt") return "🔧";
    if (name === "LICENSE") return "📜";
    if (name === ".gitignore") return "🚫";
    return "📄";
  };

  const isTextFile = (name) => {
    const exts = [".m", ".txt", ".md", ".json", ".cpp", ".hpp", ".h", ".c", ".py", ".js",
      ".ts", ".jsx", ".tsx", ".css", ".html", ".yml", ".yaml", ".toml", ".sh", ".bash",
      ".cmake", ".cfg", ".ini", ".xml", ".csv", ".r", ".jl", ".lua", ".rb", ".go",
      ".rs", ".java", ".kt", ".swift", ".pl"];
    const names = ["Makefile", "CMakeLists.txt", "LICENSE", ".gitignore", "README",
      "Dockerfile", ".env", "Gemfile", "Rakefile"];
    return exts.some(e => name.endsWith(e)) || names.some(n => name === n || name.startsWith(n));
  };

  const isMFile = (name) => name.endsWith(".m");

  const formatSize = (bytes) => {
    if (!bytes) return "";
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  };

  const renderNode = (nodeMap, depth = 0) => {
    const entries = Object.values(nodeMap);
    // Sort: directories first, then files, alphabetical
    entries.sort((a, b) => {
      if (a.type === "tree" && b.type !== "tree") return -1;
      if (a.type !== "tree" && b.type === "tree") return 1;
      return a.name.localeCompare(b.name);
    });

    return entries.map((node) => {
      const isDir = node.type === "tree";
      const isExp = expanded[node.path];
      const isSelected = previewFile === node.path;
      const isM = isMFile(node.name);

      return (
        <div key={node.path}>
          <div
            onClick={() => {
              if (isDir) {
                toggleExpand(node.path);
              } else if (isTextFile(node.name)) {
                fetchFileContent(node.path);
              }
            }}
            style={{
              display: "flex", alignItems: "center", gap: 5, padding: "3px 8px",
              paddingLeft: depth * 16 + 8, cursor: "pointer", fontSize: 12,
              background: isSelected ? `${C.accent}18` : "transparent",
              borderLeft: isSelected ? `2px solid ${C.accent}` : "2px solid transparent",
              color: isM ? C.green : isDir ? C.accent : C.textDim,
              transition: "all 0.1s",
            }}
            onMouseEnter={e => { if (!isSelected) e.currentTarget.style.background = `${C.bg3}`; }}
            onMouseLeave={e => { if (!isSelected) e.currentTarget.style.background = "transparent"; }}
          >
            {isDir ? (
              <span style={{ fontSize: 10, width: 14, textAlign: "center", color: C.textMuted }}>
                {isExp ? "▼" : "▶"}
              </span>
            ) : (
              <span style={{ fontSize: 12, width: 14, textAlign: "center" }}>
                {getFileIcon(node.name)}
              </span>
            )}
            <span style={{ flex: 1, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
              {isDir ? "📁 " : ""}{node.name}
            </span>
            {!isDir && node.size && (
              <span style={{ fontSize: 9, color: C.textMuted, flexShrink: 0 }}>{formatSize(node.size)}</span>
            )}
            {isM && (
              <span style={{
                fontSize: 8, padding: "1px 4px", borderRadius: 3,
                background: `${C.green}22`, color: C.green, flexShrink: 0,
              }}>MLab</span>
            )}
          </div>
          {isDir && isExp && Object.keys(node.children).length > 0 && renderNode(node.children, depth + 1)}
        </div>
      );
    });
  };

  const nestedTree = useMemo(() => tree ? buildTree(tree) : {}, [tree]);
  const mFileCount = useMemo(() => tree ? tree.filter(f => f.path.endsWith(".m")).length : 0, [tree]);

  if (!visible) return null;

  return (
    <div style={{
      width: 320, minWidth: 320, flexShrink: 0,
      background: C.bg1, borderRight: `1px solid ${C.border}`,
      display: "flex", flexDirection: "column", overflow: "hidden",
    }}>
      {/* Header */}
      <div style={{
        padding: "10px 12px", borderBottom: `1px solid ${C.border}`,
        display: "flex", justifyContent: "space-between", alignItems: "center", flexShrink: 0,
      }}>
        <span style={{ fontSize: 13, fontWeight: 600, color: C.text }}>GitHub Repository</span>
        <button onClick={onClose} style={{
          background: "none", border: "none", color: C.textMuted, cursor: "pointer", fontSize: 18,
        }}>×</button>
      </div>

      {/* URL input */}
      <div style={{ padding: "8px 12px", borderBottom: `1px solid ${C.border}`, flexShrink: 0 }}>
        <div style={{ display: "flex", gap: 6 }}>
          <input
            value={repoUrl}
            onChange={e => setRepoUrl(e.target.value)}
            onKeyDown={e => e.key === "Enter" && fetchRepo()}
            placeholder="owner/repo"
            style={{
              flex: 1, padding: "6px 10px", borderRadius: 6, fontSize: 12,
              background: C.bg0, border: `1px solid ${C.border}`, color: C.text,
              outline: "none", fontFamily: "'IBM Plex Mono', monospace",
            }}
          />
          <button onClick={fetchRepo} disabled={loading || !repoUrl.trim()}
            style={{
              padding: "6px 12px", borderRadius: 6, fontSize: 11, fontWeight: 600,
              background: C.accent, color: "#fff", border: "none", cursor: "pointer",
              opacity: loading || !repoUrl.trim() ? 0.5 : 1,
              fontFamily: "'IBM Plex Mono', monospace",
            }}>
            {loading ? "..." : "Load"}
          </button>
        </div>

        {/* Branch selector */}
        {branches.length > 0 && (
          <div style={{ marginTop: 6, display: "flex", alignItems: "center", gap: 6 }}>
            <span style={{ fontSize: 10, color: C.textMuted }}>Branch:</span>
            <select value={branch} onChange={e => fetchBranch(e.target.value)}
              style={{
                flex: 1, padding: "3px 6px", borderRadius: 4, fontSize: 11,
                background: C.bg0, border: `1px solid ${C.border}`, color: C.text,
                fontFamily: "'IBM Plex Mono', monospace", cursor: "pointer",
              }}>
              {branches.map(b => <option key={b} value={b}>{b}</option>)}
            </select>
          </div>
        )}

        {/* Repo info */}
        {repoInfo && (
          <div style={{ marginTop: 6, fontSize: 10, color: C.textMuted, display: "flex", gap: 8, flexWrap: "wrap" }}>
            <span>⭐ {repoInfo.stargazers_count}</span>
            <span>🍴 {repoInfo.forks_count}</span>
            {repoInfo.language && <span>💻 {repoInfo.language}</span>}
            {mFileCount > 0 && <span style={{ color: C.green }}>📄 {mFileCount} .m files</span>}
          </div>
        )}

        {error && <div style={{ color: C.red, fontSize: 11, marginTop: 6 }}>{error}</div>}
      </div>

      {/* File tree */}
      <div style={{ flex: 1, overflowY: "auto", overflowX: "hidden" }}>
        {loading && !tree && (
          <div style={{ padding: 20, textAlign: "center" }}>
            <div style={{ color: C.textMuted, fontSize: 12 }}>Loading repository...</div>
            <div style={{
              width: 30, height: 30, border: `3px solid ${C.border}`,
              borderTop: `3px solid ${C.accent}`, borderRadius: "50%",
              margin: "12px auto", animation: "spin 0.8s linear infinite",
            }} />
            <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
          </div>
        )}
        {tree && Object.keys(nestedTree).length > 0 && (
          <div style={{ padding: "4px 0" }}>{renderNode(nestedTree)}</div>
        )}
        {tree && Object.keys(nestedTree).length === 0 && (
          <div style={{ padding: 20, textAlign: "center", color: C.textMuted, fontSize: 12 }}>
            Repository is empty
          </div>
        )}
        {!tree && !loading && (
          <div style={{ padding: 20, textAlign: "center", color: C.textMuted, fontSize: 11, lineHeight: 1.6 }}>
            Enter a GitHub repo URL or owner/repo to browse its files.
            <br /><br />
            <span style={{ color: C.textDim }}>Examples:</span>
            <br />
            <span style={{ color: C.accent, cursor: "pointer" }}
              onClick={() => setRepoUrl("mathworks/MATLAB-Simulink-Challenge")}>
              mathworks/MATLAB-Simulink-Challenge
            </span>
          </div>
        )}
      </div>

      {/* File preview */}
      {previewFile && (
        <div style={{
          borderTop: `1px solid ${C.border}`, flexShrink: 0,
          maxHeight: "40%", display: "flex", flexDirection: "column",
        }}>
          <div style={{
            display: "flex", justifyContent: "space-between", alignItems: "center",
            padding: "6px 12px", background: C.bg0, borderBottom: `1px solid ${C.border}`,
          }}>
            <span style={{ fontSize: 11, color: C.text, fontWeight: 600, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
              {previewFile.split("/").pop()}
            </span>
            <div style={{ display: "flex", gap: 4, flexShrink: 0 }}>
              {isMFile(previewFile) && (
                <>
                  <button onClick={() => onRunFile(previewFile.split("/").pop(), previewContent)}
                    style={{
                      padding: "3px 8px", borderRadius: 4, fontSize: 10, fontWeight: 600,
                      background: C.green, color: C.bg0, border: "none", cursor: "pointer",
                    }}>▶ Run</button>
                  <button onClick={() => onOpenFile(previewFile.split("/").pop(), previewContent)}
                    style={{
                      padding: "3px 8px", borderRadius: 4, fontSize: 10, fontWeight: 600,
                      background: C.accent, color: "#fff", border: "none", cursor: "pointer",
                    }}>Open in Editor</button>
                </>
              )}
              {!isMFile(previewFile) && isTextFile(previewFile) && (
                <button onClick={() => onOpenFile(previewFile.split("/").pop(), previewContent)}
                  style={{
                    padding: "3px 8px", borderRadius: 4, fontSize: 10, fontWeight: 600,
                    background: C.accent, color: "#fff", border: "none", cursor: "pointer",
                  }}>Open in Editor</button>
              )}
              <button onClick={() => { setPreviewFile(null); setPreviewContent(""); }}
                style={{
                  padding: "3px 6px", borderRadius: 4, fontSize: 14, lineHeight: 1,
                  background: "none", color: C.textMuted, border: "none", cursor: "pointer",
                }}>×</button>
            </div>
          </div>
          <pre style={{
            flex: 1, overflowY: "auto", padding: "8px 12px", margin: 0,
            fontSize: 11, lineHeight: 1.5, color: C.textDim, background: C.bg0,
            fontFamily: "'IBM Plex Mono', monospace", whiteSpace: "pre-wrap", wordBreak: "break-word",
          }}>
            {previewLoading ? "Loading..." : previewContent}
          </pre>
        </div>
      )}
    </div>
  );
}

// ════════════════════════════════════════════════════════════════
// Main App
// ════════════════════════════════════════════════════════════════
export default function MLabREPL({ engine: engineProp, status: statusProp, initMessage }) {
  const [output, setOutput] = useState([]);
  const [inputVal, setInputVal] = useState("");
  const [history, setHistory] = useState([]);
  const [histIdx, setHistIdx] = useState(-1);
  const [savedInput, setSavedInput] = useState("");
  const [showExamples, setShowExamples] = useState(false);
  const [showCheatSheet, setShowCheatSheet] = useState(false);
  const [showInspector, setShowInspector] = useState(false);
  const [showGitPanel, setShowGitPanel] = useState(false);
  const [helpTopic, setHelpTopic] = useState(null);
  const [plots, setPlots] = useState([]);
  const [tabs, setTabs] = useState([{ id: "1", name: "untitled.m", code: "", modified: false }]);
  const [activeTab, setActiveTab] = useState("1");
  const [showEditor, setShowEditor] = useState(false);
  const [acItems, setAcItems] = useState([]);
  const [acIdx, setAcIdx] = useState(-1);
  const [acPartial, setAcPartial] = useState("");
  const [execTimeMs, setExecTimeMs] = useState(null);

  const [showConsole, setShowConsole] = useState(true);

  const interpreterRef = useRef(null);
  const outputRef = useRef(null);
  const inputRef = useRef(null);
  const tabCountRef = useRef(1);

  if (!interpreterRef.current) {
    interpreterRef.current = createInterpreter();
  }
  const interp = interpreterRef.current;

  const scrollBottom = useCallback(() => {
    requestAnimationFrame(() => {
      if (outputRef.current) outputRef.current.scrollTop = outputRef.current.scrollHeight;
    });
  }, []);

  useEffect(() => {
    setOutput([{ type: "system", text: "MLab REPL v2.1 — Enhanced Web IDE" }, { type: "system", text: 'Type commands below. "help <topic>" for function info. Click 🐙 Git Repo to browse GitHub.' }]);
  }, []);

  useEffect(scrollBottom, [output, plots]);

  const addOutput = useCallback((items) => {
    setOutput(prev => {
      // Check for __CLEAR__
      for (const item of items) {
        if (item.text === "__CLEAR__") return [];
      }
      return [...prev, ...items.filter(i => i.text !== "__CLEAR__")];
    });
  }, []);

  const runCode = useCallback((code) => {
    const t0 = performance.now();
    const result = interp.execute(code);
    setExecTimeMs(performance.now() - t0);

    const items = [];
    if (result.output) {
      for (const line of result.output.split("\n")) {
        if (line === "__CLEAR__") { setOutput([]); continue; }
        if (/^Error:/.test(line)) items.push({ type: "error", text: line });
        else if (/^Warning:/.test(line)) items.push({ type: "warning", text: line });
        else items.push({ type: "result", text: line });
      }
    }
    addOutput(items);
    if (result.plot) {
      setPlots(prev => [...prev, result.plot]);
    }
  }, [interp, addOutput]);

  const handleSubmit = useCallback(() => {
    const val = inputVal.trim();
    if (!val) return;
    addOutput([{ type: "input", text: val }]);
    setHistory(prev => {
      const h = [...prev, val];
      return h.length > 200 ? h.slice(-200) : h;
    });
    setHistIdx(-1);
    setInputVal("");
    setAcItems([]);

    // Check for help inline
    const hm = val.match(/^help\s+(\w+)$/);
    if (hm && HELP_DB[hm[1]]) {
      setHelpTopic(hm[1]);
      return;
    }
    if (val === "help") {
      setHelpTopic(null);
      addOutput([{ type: "system", text: "Commands: clc, clear, who, whos, help <topic>\nKeys: Enter=exec, Shift+Enter=newline, Tab=autocomplete, ↑↓=history\nType 'help <function>' for detailed info (e.g. help sin)" }]);
      return;
    }

    runCode(val);
  }, [inputVal, addOutput, runCode]);

  const handleKeyDown = useCallback((e) => {
    // Autocomplete navigation
    if (acItems.length > 0) {
      if (e.key === "ArrowDown") { e.preventDefault(); setAcIdx(i => (i + 1) % acItems.length); return; }
      if (e.key === "ArrowUp") { e.preventDefault(); setAcIdx(i => (i - 1 + acItems.length) % acItems.length); return; }
      if (e.key === "Enter" || e.key === "Tab") {
        if (acIdx >= 0) {
          e.preventDefault();
          const item = acItems[acIdx];
          const val = inputVal;
          const cur = inputRef.current?.selectionStart || val.length;
          let ws = cur - 1;
          while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
          ws++;
          setInputVal(val.substring(0, ws) + item + val.substring(cur));
          setAcItems([]);
          return;
        }
      }
      if (e.key === "Escape") { setAcItems([]); return; }
    }

    if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); handleSubmit(); return; }
    if (e.key === "ArrowUp" && !e.shiftKey && !inputVal.includes("\n")) {
      e.preventDefault();
      if (!history.length) return;
      const newIdx = histIdx === -1 ? history.length - 1 : Math.max(0, histIdx - 1);
      if (histIdx === -1) setSavedInput(inputVal);
      setHistIdx(newIdx);
      setInputVal(history[newIdx]);
      return;
    }
    if (e.key === "ArrowDown" && !e.shiftKey && !inputVal.includes("\n")) {
      e.preventDefault();
      if (histIdx === -1) return;
      if (histIdx < history.length - 1) {
        const newIdx = histIdx + 1;
        setHistIdx(newIdx);
        setInputVal(history[newIdx]);
      } else {
        setHistIdx(-1);
        setInputVal(savedInput);
      }
      return;
    }
    if (e.key === "Tab") {
      e.preventDefault();
      const val = inputVal;
      const cur = inputRef.current?.selectionStart || val.length;
      let ws = cur - 1;
      while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
      ws++;
      const partial = val.substring(ws, cur);
      if (partial) {
        const items = interp.complete(partial);
        if (items.length === 1) {
          setInputVal(val.substring(0, ws) + items[0] + val.substring(cur));
          setAcItems([]);
        } else if (items.length > 1) {
          setAcItems(items);
          setAcIdx(0);
          setAcPartial(partial);
        }
      }
      return;
    }
    if (e.key === "l" && e.ctrlKey) { e.preventDefault(); setOutput([]); setPlots([]); return; }
    if (e.key === "c" && e.ctrlKey) {
      if (inputRef.current?.selectionStart === inputRef.current?.selectionEnd) {
        addOutput([{ type: "input", text: inputVal + "^C" }]);
        setInputVal("");
      }
    }
  }, [inputVal, handleSubmit, history, histIdx, savedInput, acItems, acIdx, interp, addOutput]);

  const runExample = useCallback((item) => {
    setShowExamples(false);
    addOutput([{ type: "system", text: `── ${item.title} ──` }]);
    addOutput([{ type: "input", text: item.code }]);
    runCode(item.code);
    addOutput([{ type: "system", text: "" }]);
    inputRef.current?.focus();
  }, [addOutput, runCode]);

  // Tab operations
  const newTab = useCallback(() => {
    tabCountRef.current++;
    const id = String(tabCountRef.current);
    setTabs(prev => [...prev, { id, name: `script${tabCountRef.current}.m`, code: "", modified: false }]);
    setActiveTab(id);
  }, []);

  const closeTab = useCallback((id) => {
    setTabs(prev => {
      const next = prev.filter(t => t.id !== id);
      if (!next.length) return prev;
      if (activeTab === id) setActiveTab(next[next.length - 1].id);
      return next;
    });
  }, [activeTab]);

  const renameTab = useCallback((id, name) => {
    if (!name.trim()) return;
    setTabs(prev => prev.map(t => t.id === id ? { ...t, name: name.trim() } : t));
  }, []);

  const activeTabData = tabs.find(t => t.id === activeTab) || tabs[0];

  const updateTabCode = useCallback((code) => {
    setTabs(prev => prev.map(t => t.id === activeTab ? { ...t, code, modified: true } : t));
  }, [activeTab]);

  const runActiveTab = useCallback(() => {
    const tab = tabs.find(t => t.id === activeTab);
    if (!tab || !tab.code.trim()) return;
    addOutput([{ type: "system", text: `── Running ${tab.name} ──` }]);
    addOutput([{ type: "input", text: tab.code }]);
    runCode(tab.code);
    addOutput([{ type: "system", text: "" }]);
    setTabs(prev => prev.map(t => t.id === activeTab ? { ...t, modified: false } : t));
  }, [tabs, activeTab, addOutput, runCode]);

  // File I/O
  const handleFileLoad = useCallback(() => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".m,.txt";
    input.onchange = (e) => {
      const file = e.target.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = (ev) => {
        const code = ev.target.result;
        tabCountRef.current++;
        const id = String(tabCountRef.current);
        setTabs(prev => [...prev, { id, name: file.name, code, modified: false }]);
        setActiveTab(id);
        setShowEditor(true);
      };
      reader.readAsText(file);
    };
    input.click();
  }, []);

  const handleFileSave = useCallback(() => {
    const tab = tabs.find(t => t.id === activeTab);
    if (!tab) return;
    const blob = new Blob([tab.code], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = tab.name;
    a.click();
    URL.revokeObjectURL(url);
    setTabs(prev => prev.map(t => t.id === activeTab ? { ...t, modified: false } : t));
  }, [tabs, activeTab]);

  // Git repo file handlers
  const handleGitOpenFile = useCallback((filename, content) => {
    tabCountRef.current++;
    const id = String(tabCountRef.current);
    setTabs(prev => [...prev, { id, name: filename, code: content, modified: false }]);
    setActiveTab(id);
    setShowEditor(true);
    addOutput([{ type: "system", text: `Opened ${filename} from GitHub` }]);
  }, [addOutput]);

  const handleGitRunFile = useCallback((filename, content) => {
    addOutput([{ type: "system", text: `── Running ${filename} (from GitHub) ──` }]);
    addOutput([{ type: "input", text: content }]);
    runCode(content);
    addOutput([{ type: "system", text: "" }]);
  }, [addOutput, runCode]);

  const variables = interp.getVars();

  const font = "'IBM Plex Mono', 'JetBrains Mono', 'Fira Code', 'SF Mono', Consolas, monospace";

  // Toolbar button component
  const Btn = ({ onClick, active, children, title }) => (
    <button onClick={onClick} title={title} style={{
      display: "flex", alignItems: "center", gap: 5, padding: "5px 10px",
      border: `1px solid ${active ? C.accent : C.border}`, borderRadius: 6,
      background: active ? `${C.accent}22` : C.bg3,
      color: active ? C.accent : C.textDim,
      fontFamily: font, fontSize: 11, cursor: "pointer",
      transition: "all 0.15s", whiteSpace: "nowrap",
    }}>{children}</button>
  );

  return (
    <div style={{
      display: "flex", flexDirection: "column", height: "100vh", width: "100%",
      background: C.bg0, color: C.text, fontFamily: font, fontSize: 13, overflow: "hidden",
    }}>
      {/* Header */}
      <div style={{
        display: "flex", alignItems: "center", justifyContent: "space-between",
        padding: "8px 16px", background: C.bg1, borderBottom: `1px solid ${C.border}`,
        flexShrink: 0, zIndex: 30, flexWrap: "wrap", gap: 6,
      }}>
        <div style={{ display: "flex", alignItems: "baseline", gap: 8 }}>
          <span style={{ fontSize: 16, fontWeight: 700, letterSpacing: -0.5 }}>
            MLab <span style={{ color: C.accent }}>REPL</span>
          </span>
          <span style={{ fontSize: 10, color: C.textMuted }}>v2.1</span>
        </div>
        <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
          <Btn onClick={() => setShowGitPanel(!showGitPanel)} active={showGitPanel} title="Browse GitHub Repo">🐙 Git Repo</Btn>
          <Btn onClick={() => setShowExamples(!showExamples)} active={showExamples} title="Examples">📋 Examples</Btn>
          <Btn onClick={() => setShowCheatSheet(!showCheatSheet)} active={showCheatSheet} title="Quick Reference">📖 Cheat Sheet</Btn>
          <Btn onClick={() => setShowEditor(!showEditor)} active={showEditor} title="Script Editor">📝 Editor</Btn>
          <Btn onClick={() => setShowConsole(!showConsole)} active={showConsole} title="Toggle Console">💻 Console</Btn>
          <Btn onClick={() => setShowInspector(!showInspector)} active={showInspector} title="Variable Inspector">🔍 Workspace</Btn>
          {showEditor && (
            <Btn onClick={runActiveTab} title="Run current script">
              <span style={{ color: C.green }}>▶</span> Run
            </Btn>
          )}
          <Btn onClick={handleFileLoad} title="Open .m file">📂 Open</Btn>
          {showEditor && <Btn onClick={handleFileSave} title="Save .m file">💾 Save</Btn>}
          <Btn onClick={() => { setOutput([]); setPlots([]); }} title="Clear screen">🗑 Clear</Btn>
          <Btn onClick={() => { interp.reset(); setOutput(prev => [...prev, { type: "system", text: "Workspace cleared." }]); }} title="Reset workspace">🔄 Reset</Btn>
        </div>
      </div>

      {/* Examples (dropdown) */}
      <ExamplesPanel visible={showExamples} onClose={() => setShowExamples(false)} onRun={runExample} />

      {/* Cheat Sheet (dropdown) */}
      <CheatSheet visible={showCheatSheet} onClose={() => setShowCheatSheet(false)} />

      {/* Main area: horizontal flex [Git?] [Center] [Workspace?] */}
      <div style={{ flex: 1, display: "flex", overflow: "hidden" }}>

        {/* Left: Git Repo Browser */}
        {showGitPanel && (
          <GitRepoBrowser
            visible={showGitPanel}
            onClose={() => setShowGitPanel(false)}
            onOpenFile={handleGitOpenFile}
            onRunFile={handleGitRunFile}
          />
        )}

        {/* Center column: vertical flex [Editor?] [Console?] */}
        <div style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden", minWidth: 0 }}>

          {/* Editor */}
          {showEditor && (
            <div style={{
              flex: showConsole ? "0 0 50%" : 1,
              display: "flex", flexDirection: "column",
              borderBottom: showConsole ? `2px solid ${C.border}` : "none",
              overflow: "hidden",
            }}>
              <TabBar tabs={tabs} activeTab={activeTab} onSelect={setActiveTab}
                onClose={closeTab} onNew={newTab} onRename={renameTab} />
              <div style={{ flex: 1, display: "flex", overflow: "hidden" }}>
                <div style={{
                  padding: "8px 0", background: C.bg0, borderRight: `1px solid ${C.border}`,
                  userSelect: "none", minWidth: 36, textAlign: "right", overflowY: "auto",
                }}>
                  {(activeTabData?.code || "").split("\n").map((_, i) => (
                    <div key={i} style={{ fontSize: 11, color: C.textMuted, padding: "0 8px", lineHeight: "20px" }}>{i + 1}</div>
                  ))}
                </div>
                <textarea
                  value={activeTabData?.code || ""}
                  onChange={e => updateTabCode(e.target.value)}
                  spellCheck={false}
                  style={{
                    flex: 1, background: C.bg1, color: C.text, border: "none", outline: "none",
                    fontFamily: font, fontSize: 13, lineHeight: "20px", padding: 8, resize: "none",
                    caretColor: C.accent, overflow: "auto",
                  }}
                />
              </div>
            </div>
          )}

          {/* Console (terminal output + input) */}
          {showConsole && (
            <div style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden" }}>
              {/* Output */}
              <div ref={outputRef} style={{
                flex: 1, overflowY: "auto", padding: "12px 16px", background: C.bg1,
              }}>
                {output.map((item, i) => {
                  const colors = {
                    input: C.textMuted, result: C.text, error: C.red,
                    warning: C.orange, system: C.textMuted, info: C.cyan,
                  };
                  if (item.type === "input") {
                    return (
                      <div key={i} style={{ padding: "1px 0", whiteSpace: "pre-wrap", wordBreak: "break-word" }}>
                        <span style={{ color: C.green, fontWeight: 700, userSelect: "none" }}>{">> "}</span>
                        <span style={{ color: C.textMuted }}>{item.text}</span>
                      </div>
                    );
                  }
                  return (
                    <div key={i} style={{
                      padding: "1px 0", color: colors[item.type] || C.text,
                      fontStyle: item.type === "system" ? "italic" : "normal",
                      whiteSpace: "pre-wrap", wordBreak: "break-word",
                    }}>{item.text}</div>
                  );
                })}

                {helpTopic && <HelpPanel topic={helpTopic} onClose={() => setHelpTopic(null)} />}

                {plots.map((p, i) => (
                  <PlotPanel key={i} data={p} onClose={() => setPlots(prev => prev.filter((_, j) => j !== i))} />
                ))}
              </div>

              {/* Input line */}
              <div style={{
                display: "flex", alignItems: "flex-start", padding: "10px 16px",
                background: C.bg0, borderTop: `1px solid ${C.border}`, flexShrink: 0,
                position: "relative",
              }}>
                <span style={{ color: C.green, fontWeight: 700, marginRight: 8, marginTop: 2, userSelect: "none", flexShrink: 0 }}>&gt;&gt;</span>
                <div style={{ flex: 1, position: "relative" }}>
                  {acItems.length > 1 && (
                    <div style={{
                      position: "absolute", bottom: "calc(100% + 4px)", left: 0,
                      minWidth: 180, maxWidth: 360, maxHeight: 180, overflowY: "auto",
                      background: C.bg3, border: `1px solid ${C.border}`, borderRadius: 6,
                      boxShadow: "0 -4px 16px rgba(0,0,0,0.4)", zIndex: 100,
                    }}>
                      {acItems.map((item, i) => (
                        <div key={item}
                          onClick={() => {
                            const val = inputVal;
                            const cur = inputRef.current?.selectionStart || val.length;
                            let ws = cur - 1;
                            while (ws >= 0 && /[a-zA-Z0-9_]/.test(val[ws])) ws--;
                            ws++;
                            setInputVal(val.substring(0, ws) + item + val.substring(cur));
                            setAcItems([]);
                            inputRef.current?.focus();
                          }}
                          style={{
                            padding: "5px 10px", cursor: "pointer", fontSize: 12,
                            color: i === acIdx ? C.text : C.textDim,
                            background: i === acIdx ? C.border : "transparent",
                          }}>
                          <span style={{ color: C.accent, fontWeight: 600 }}>{item.substring(0, acPartial.length)}</span>
                          {item.substring(acPartial.length)}
                        </div>
                      ))}
                    </div>
                  )}
                  <textarea
                    ref={inputRef}
                    value={inputVal}
                    onChange={e => { setInputVal(e.target.value); setAcItems([]); }}
                    onKeyDown={handleKeyDown}
                    rows={1}
                    spellCheck={false}
                    autoComplete="off"
                    placeholder="Enter MLab command..."
                    style={{
                      width: "100%", background: "transparent", border: "none", outline: "none",
                      color: C.text, fontFamily: font, fontSize: 13, lineHeight: 1.6,
                      resize: "none", overflow: "hidden", caretColor: C.accent,
                    }}
                    onInput={e => { e.target.style.height = "auto"; e.target.style.height = e.target.scrollHeight + "px"; }}
                  />
                </div>
              </div>
            </div>
          )}

          {/* If both editor and console are hidden, show a placeholder */}
          {!showEditor && !showConsole && (
            <div style={{
              flex: 1, display: "flex", alignItems: "center", justifyContent: "center",
              color: C.textMuted, fontSize: 12,
            }}>
              Open Editor or Console from the toolbar above
            </div>
          )}
        </div>

        {/* Right: Variable Inspector */}
        {showInspector && (
          <VarInspector variables={variables} visible={showInspector} onClose={() => setShowInspector(false)} />
        )}
      </div>

      {/* Status bar */}
      <div style={{
        display: "flex", justifyContent: "space-between", alignItems: "center",
        padding: "4px 16px", background: C.bg0, borderTop: `1px solid ${C.border}`,
        fontSize: 10, color: C.textMuted, flexShrink: 0,
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
          <span style={{
            width: 7, height: 7, borderRadius: "50%",
            background: statusProp === "ready" ? C.green : C.yellow,
            display: "inline-block",
          }} />
          <span>{statusProp === "ready" ? "Ready (WASM)" : "Ready (demo mode)"}</span>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
          {execTimeMs !== null && <span>{execTimeMs.toFixed(1)}ms</span>}
          <span style={{ color: C.border }}>|</span>
          <span>Tab: autocomplete</span>
          <span style={{ color: C.border }}>|</span>
          <span>↑↓: history</span>
          <span style={{ color: C.border }}>|</span>
          <span>Shift+Enter: newline</span>
        </div>
      </div>
    </div>
  );
}
