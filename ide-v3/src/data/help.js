/** Built-in help entries keyed by function name */
const HELP_DB = {
  // ── Math ──
  sin:      { sig: 'sin(x)',           desc: 'Sine of x (radians)',                  cat: 'Math',        ex: 'sin(pi/2)  % → 1' },
  cos:      { sig: 'cos(x)',           desc: 'Cosine of x (radians)',                cat: 'Math',        ex: 'cos(0)  % → 1' },
  tan:      { sig: 'tan(x)',           desc: 'Tangent of x (radians)',               cat: 'Math',        ex: 'tan(pi/4)  % → 1' },
  asin:     { sig: 'asin(x)',          desc: 'Inverse sine, result in radians',      cat: 'Math',        ex: 'asin(1)  % → pi/2' },
  acos:     { sig: 'acos(x)',          desc: 'Inverse cosine, result in radians',    cat: 'Math',        ex: 'acos(1)  % → 0' },
  atan:     { sig: 'atan(x)',          desc: 'Inverse tangent, result in radians',   cat: 'Math',        ex: 'atan(1)  % → pi/4' },
  sqrt:     { sig: 'sqrt(x)',          desc: 'Square root of x',                     cat: 'Math',        ex: 'sqrt(144)  % → 12' },
  abs:      { sig: 'abs(x)',           desc: 'Absolute value of x',                  cat: 'Math',        ex: 'abs(-7)  % → 7' },
  exp:      { sig: 'exp(x)',           desc: 'Exponential e^x',                      cat: 'Math',        ex: 'exp(1)  % → 2.7183' },
  log:      { sig: 'log(x)',           desc: 'Natural logarithm of x',              cat: 'Math',        ex: 'log(exp(5))  % → 5' },
  log2:     { sig: 'log2(x)',          desc: 'Base-2 logarithm',                     cat: 'Math',        ex: 'log2(8)  % → 3' },
  log10:    { sig: 'log10(x)',         desc: 'Base-10 logarithm',                    cat: 'Math',        ex: 'log10(100)  % → 2' },
  floor:    { sig: 'floor(x)',         desc: 'Round toward negative infinity',       cat: 'Math',        ex: 'floor(3.7)  % → 3' },
  ceil:     { sig: 'ceil(x)',          desc: 'Round toward positive infinity',       cat: 'Math',        ex: 'ceil(-2.3)  % → -2' },
  round:    { sig: 'round(x)',         desc: 'Round to nearest integer',             cat: 'Math',        ex: 'round(3.5)  % → 4' },
  mod:      { sig: 'mod(a, b)',        desc: 'Modulus after division',               cat: 'Math',        ex: 'mod(17, 5)  % → 2' },
  rem:      { sig: 'rem(a, b)',        desc: 'Remainder after division',             cat: 'Math',        ex: 'rem(17, 5)  % → 2' },
  pow:      { sig: 'pow(x, y)',        desc: 'x raised to power y',                 cat: 'Math',        ex: 'pow(2, 10)  % → 1024' },
  sign:     { sig: 'sign(x)',          desc: 'Sign of x (-1, 0, or 1)',             cat: 'Math',        ex: 'sign(-42)  % → -1' },

  // ── Matrix ──
  zeros:    { sig: 'zeros(n) / zeros(m,n)', desc: 'Create matrix of zeros',         cat: 'Matrix',      ex: 'zeros(2,3)' },
  ones:     { sig: 'ones(n) / ones(m,n)',   desc: 'Create matrix of ones',          cat: 'Matrix',      ex: 'ones(1,4)' },
  eye:      { sig: 'eye(n)',           desc: 'Identity matrix',                      cat: 'Matrix',      ex: 'eye(3)' },
  rand:     { sig: 'rand(n) / rand(m,n)',   desc: 'Uniform random matrix [0,1)',     cat: 'Matrix',      ex: 'rand(2,3)' },
  randn:    { sig: 'randn(n) / randn(m,n)', desc: 'Normal random matrix',           cat: 'Matrix',      ex: 'randn(3)' },
  linspace: { sig: 'linspace(a, b, n)',     desc: 'n linearly spaced points a→b',   cat: 'Matrix',      ex: 'linspace(0, 1, 5)' },
  logspace: { sig: 'logspace(a, b, n)',     desc: 'n log-spaced points 10^a→10^b',  cat: 'Matrix',      ex: 'logspace(0, 3, 4)' },
  reshape:  { sig: 'reshape(A, m, n)',      desc: 'Reshape matrix to m×n',          cat: 'Matrix',      ex: 'reshape(1:6, 2, 3)' },
  size:     { sig: 'size(A)',          desc: 'Dimensions of matrix A',               cat: 'Matrix',      ex: 'size([1 2; 3 4])' },
  length:   { sig: 'length(A)',        desc: 'Length of largest dimension',           cat: 'Matrix',      ex: 'length([1 2 3 4])' },
  numel:    { sig: 'numel(A)',         desc: 'Total number of elements',             cat: 'Matrix',      ex: 'numel([1 2; 3 4])' },

  // ── Aggregation ──
  sum:      { sig: 'sum(A)',           desc: 'Sum of elements',                      cat: 'Aggregation', ex: 'sum([1 2 3 4 5])  % → 15' },
  prod:     { sig: 'prod(A)',          desc: 'Product of elements',                  cat: 'Aggregation', ex: 'prod([1 2 3 4])  % → 24' },
  mean:     { sig: 'mean(A)',          desc: 'Average of elements',                  cat: 'Aggregation', ex: 'mean([2 4 6])  % → 4' },
  min:      { sig: 'min(A)',           desc: 'Minimum element',                      cat: 'Aggregation', ex: 'min([5 1 3])  % → 1' },
  max:      { sig: 'max(A)',           desc: 'Maximum element',                      cat: 'Aggregation', ex: 'max([5 1 3])  % → 5' },
  cumsum:   { sig: 'cumsum(A)',        desc: 'Cumulative sum',                       cat: 'Aggregation', ex: 'cumsum([1 2 3])  % → [1 3 6]' },
  sort:     { sig: 'sort(A)',          desc: 'Sort in ascending order',              cat: 'Aggregation', ex: 'sort([3 1 2])  % → [1 2 3]' },

  // ── String ──
  upper:    { sig: 'upper(s)',         desc: 'Convert string to uppercase',          cat: 'String',      ex: "upper('hello')  % → 'HELLO'" },
  lower:    { sig: 'lower(s)',         desc: 'Convert string to lowercase',          cat: 'String',      ex: "lower('MATLAB')  % → 'matlab'" },
  strcmp:   { sig: 'strcmp(s1, s2)',    desc: 'Compare strings (case-sensitive)',     cat: 'String',      ex: "strcmp('a', 'a')  % → 1" },
  strcmpi:  { sig: 'strcmpi(s1, s2)', desc: 'Compare strings (case-insensitive)',   cat: 'String',      ex: "strcmpi('A', 'a')  % → 1" },
  strcat:   { sig: 'strcat(s1, s2, ...)', desc: 'Concatenate strings',              cat: 'String',      ex: "strcat('he','llo')" },
  strsplit: { sig: 'strsplit(s, delim)',   desc: 'Split string by delimiter',        cat: 'String',      ex: "strsplit('a,b,c', ',')" },
  num2str:  { sig: 'num2str(x)',       desc: 'Convert number to string',             cat: 'String',      ex: 'num2str(42)' },
  sprintf:  { sig: 'sprintf(fmt, ...)',    desc: 'Formatted string output',          cat: 'String',      ex: "sprintf('x = %d', 42)" },

  // ── I/O ──
  disp:     { sig: 'disp(x)',         desc: 'Display value of x',                    cat: 'I/O',         ex: "disp('Hello')" },
  fprintf:  { sig: 'fprintf(fmt, ...)',    desc: 'Formatted output to console',      cat: 'I/O',         ex: "fprintf('x = %d\\n', 42)" },

  // ── Complex ──
  real:     { sig: 'real(z)',          desc: 'Real part of complex number',           cat: 'Complex',     ex: 'real(3+4i)  % → 3' },
  imag:     { sig: 'imag(z)',          desc: 'Imaginary part of complex number',     cat: 'Complex',     ex: 'imag(3+4i)  % → 4' },
  conj:     { sig: 'conj(z)',          desc: 'Complex conjugate',                    cat: 'Complex',     ex: 'conj(3+4i)  % → 3-4i' },

  // ── Workspace ──
  clear:    { sig: 'clear / clear all',    desc: 'Clear all variables and figures',  cat: 'Workspace',   ex: 'clear' },
  clc:      { sig: 'clc',             desc: 'Clear terminal screen',                 cat: 'Workspace',   ex: 'clc' },
  who:      { sig: 'who / who x y',   desc: 'List variable names',                   cat: 'Workspace',   ex: 'who' },
  whos:     { sig: 'whos / whos x',   desc: 'List variables with size/class',        cat: 'Workspace',   ex: 'whos' },
  which:    { sig: 'which name',       desc: 'Identify variable or function',        cat: 'Workspace',   ex: 'which sin' },
  exist:    { sig: 'exist(name)',      desc: 'Check if name exists (1=var, 5=func)', cat: 'Workspace',   ex: "exist('x')" },

  // ── Type ──
  isempty:  { sig: 'isempty(A)',      desc: 'True if A is empty',                    cat: 'Type',        ex: 'isempty([])  % → 1' },
  isnumeric:{ sig: 'isnumeric(x)',    desc: 'True if x is numeric',                 cat: 'Type',        ex: 'isnumeric(42)  % → 1' },
  ischar:   { sig: 'ischar(x)',       desc: 'True if x is a string',                cat: 'Type',        ex: "ischar('hi')  % → 1" },

  // ── Plotting — basic ──
  plot:     { sig: 'plot(x, y, style, Name, Value)',  desc: '2D line plot with optional style and properties', cat: 'Plotting', ex: "plot(x, y, 'r--o', 'LineWidth', 2)" },
  bar:      { sig: 'bar(x, y) / bar(y)',              desc: 'Bar chart',                                     cat: 'Plotting', ex: 'bar([1 4 2 5 3])' },
  scatter:  { sig: 'scatter(x, y)',                    desc: 'Scatter plot',                                  cat: 'Plotting', ex: 'scatter(rand(1,20), rand(1,20))' },
  hist:     { sig: 'hist(data, bins)',                 desc: 'Histogram',                                     cat: 'Plotting', ex: 'hist(randn(1,1000), 20)' },
  stem:     { sig: 'stem(x, y) / stem(y)',             desc: 'Discrete sequence plot (lollipop)',              cat: 'Plotting', ex: 'stem(0:10, sin(0:10))' },
  stairs:   { sig: 'stairs(x, y) / stairs(y)',         desc: 'Staircase plot (zero-order hold)',               cat: 'Plotting', ex: 'stairs(0:5, [1 3 2 4 1 3])' },
  polarplot:{ sig: 'polarplot(theta, rho, style)',     desc: 'Polar coordinate line plot',                    cat: 'Plotting', ex: 'polarplot(0:0.01:2*pi, cos(2*t))' },

  // ── Plotting — log scales ──
  semilogx: { sig: 'semilogx(x, y)',  desc: 'Plot with logarithmic X axis',         cat: 'Plotting',    ex: 'semilogx(logspace(0,3), 1:50)' },
  semilogy: { sig: 'semilogy(x, y)',  desc: 'Plot with logarithmic Y axis',          cat: 'Plotting',    ex: 'semilogy(1:10, 2.^(1:10))' },
  loglog:   { sig: 'loglog(x, y)',    desc: 'Plot with both axes logarithmic',       cat: 'Plotting',    ex: 'loglog(x, x.^2)' },

  // ── Plotting — config ──
  figure:   { sig: 'figure / figure(n)',  desc: 'Create or switch to figure window', cat: 'Plotting',    ex: 'figure(2)' },
  close:    { sig: 'close / close(n) / close all', desc: 'Close figure(s)',          cat: 'Plotting',    ex: 'close all' },
  clf:      { sig: 'clf',             desc: 'Clear current figure',                   cat: 'Plotting',    ex: 'clf' },
  hold:     { sig: 'hold on / hold off',  desc: 'Retain or replace plot data',       cat: 'Plotting',    ex: 'hold on' },
  subplot:  { sig: 'subplot(m, n, p)', desc: 'Create subplot in m×n grid at pos p',  cat: 'Plotting',    ex: 'subplot(2, 1, 1)' },
  title:    { sig: "title('text')",    desc: 'Set plot title',                       cat: 'Plotting',    ex: "title('My Plot')" },
  xlabel:   { sig: "xlabel('text')",   desc: 'Set x-axis label',                    cat: 'Plotting',    ex: "xlabel('Time')" },
  ylabel:   { sig: "ylabel('text')",   desc: 'Set y-axis label',                    cat: 'Plotting',    ex: "ylabel('Value')" },
  legend:   { sig: "legend('a','b')",  desc: 'Add legend to current axes',           cat: 'Plotting',    ex: "legend('sin', 'cos')" },
  grid:     { sig: 'grid on / grid off',  desc: 'Toggle grid lines',                cat: 'Plotting',    ex: 'grid on' },
  xlim:     { sig: 'xlim([a b])',      desc: 'Set x-axis limits',                   cat: 'Plotting',    ex: 'xlim([0 10])' },
  ylim:     { sig: 'ylim([a b])',      desc: 'Set y-axis limits',                   cat: 'Plotting',    ex: 'ylim([-1 1])' },
  rlim:     { sig: 'rlim([a b])',      desc: 'Set radial axis limits (polar)',       cat: 'Plotting',    ex: 'rlim([0 0.5])' },
  axis:     { sig: "axis('mode')",     desc: 'Set axis mode: equal, tight, ij, xy', cat: 'Plotting',    ex: "axis equal" },

  // ── Polar config ──
  thetadir: { sig: "thetadir('clockwise')", desc: 'Set polar angle direction',      cat: 'Plotting',    ex: "thetadir('clockwise')" },
  thetazero:{ sig: "thetazero('top')",      desc: 'Set polar zero angle location',  cat: 'Plotting',    ex: "thetazero('top')" },
  thetalim: { sig: 'thetalim([a b])',       desc: 'Set angular limits (degrees)',    cat: 'Plotting',    ex: 'thetalim([0 180])' },
};

export default HELP_DB;
