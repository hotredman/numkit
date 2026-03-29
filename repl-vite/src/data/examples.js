const EXAMPLES = [
  {
    category: 'Basics', icon: '📐',
    items: [
      { title: 'Arithmetic', description: 'Operator precedence, power, modulo', code: "clear\ndisp(2 + 3 * 4 - 1)\ndisp((2 + 3) * (4 - 1))\ndisp(2 ^ 3 ^ 2)\ndisp(mod(17, 5))" },
      { title: 'Variables', description: 'Assign and use variables', code: "clear\nx = 42;\ny = 3.14;\nresult = x * y;\ndisp(result)" },
      { title: 'Math functions', description: 'sqrt, abs, log, exp, rounding', code: "clear\ndisp(sqrt(144))\ndisp(abs(-7))\ndisp(log(exp(5)))\ndisp(floor(3.7))\ndisp(ceil(-2.3))" },
      { title: 'Strings', description: 'Upper/lower, length, strcmp', code: "clear\ndisp(upper('hello'))\ndisp(lower('MATLAB'))\ndisp(length('OpenAI'))\ndisp(strcmp('test', 'test'))" },
    ],
  },
  {
    category: 'Matrices', icon: '🔢',
    items: [
      { title: 'Create & index', description: 'Matrix creation and element access', code: "clear\nA = [1 2 3; 4 5 6; 7 8 9];\ndisp(A)\ndisp(A(2, 3))" },
      { title: 'Matrix arithmetic', description: 'Addition and element-wise multiply', code: "clear\nA = [1 2 3; 4 5 6; 7 8 9];\nB = [9 8 7; 6 5 4; 3 2 1];\ndisp(A + B)\ndisp(A .* B)" },
      { title: 'Special matrices', description: 'eye, zeros, ones, linspace', code: "clear\ndisp(eye(3))\ndisp(zeros(2))\ndisp(ones(1, 4))\ndisp(linspace(0, 1, 5))" },
      { title: 'Vector operations', description: 'Ranges, sum, min, max', code: "clear\nv = 1:10;\ndisp(v)\ndisp(sum(v))\ndisp(min(v))\ndisp(max(v))" },
    ],
  },
  {
    category: 'Control Flow', icon: '🔄',
    items: [
      { title: 'For loop', description: 'Sum of 1 to 100', code: "clear\ntotal = 0;\nfor i = 1:100\n    total = total + i;\nend\ndisp(total)" },
      { title: 'While loop', description: 'First power of 2 >= 1000', code: "clear\nn = 1;\nwhile n < 1000\n    n = n * 2;\nend\ndisp(n)" },
      { title: 'If / elseif / else', description: 'Conditional branching', code: "clear\nval = 42;\nif val > 100\n    disp('big')\nelseif val > 10\n    disp('medium')\nelse\n    disp('small')\nend" },
      { title: 'Nested loops', description: 'Multiplication table', code: "clear\nfor i = 1:5\n    row = '';\n    for j = 1:5\n        row = [row, num2str(i*j), ' '];\n    end\n    disp(row)\nend" },
    ],
  },
  {
    category: 'Functions', icon: '⚡',
    items: [
      { title: 'Factorial', description: 'Recursive factorial', code: "clear\nfunction y = factorial(n)\n    if n <= 1\n        y = 1;\n    else\n        y = n * factorial(n - 1);\n    end\nend\n\ndisp(factorial(10))" },
      { title: 'Fibonacci', description: 'Recursive Fibonacci sequence', code: "clear\nfunction r = fib(n)\n    if n <= 1\n        r = n;\n    else\n        r = fib(n-1) + fib(n-2);\n    end\nend\n\nresult = [];\nfor k = 0:9\n    result = [result, fib(k)];\nend\ndisp(result)" },
      { title: 'Multiple returns', description: 'Function returning min and max', code: "clear\nfunction [mn, mx] = minmax(v)\n    mn = v(1); mx = v(1);\n    for i = 2:length(v)\n        if v(i) < mn\n            mn = v(i);\n        end\n        if v(i) > mx\n            mx = v(i);\n        end\n    end\nend\n\n[lo, hi] = minmax([5 3 9 1 7]);\ndisp(lo)\ndisp(hi)" },
    ],
  },
  {
    category: 'Plotting', icon: '📊',
    items: [
      { title: 'Sine wave', description: 'Basic line plot with labels', code: "clear\nx = linspace(0, 2*pi, 100);\ny = sin(x);\nfigure\nplot(x, y)\ntitle('Sine Wave')\nxlabel('x')\nylabel('sin(x)')\ngrid on" },
      { title: 'Multiple curves', description: 'Overlay sin & cos with legend', code: "clear\nx = linspace(0, 2*pi, 100);\nfigure\nhold on\nplot(x, sin(x))\nplot(x, cos(x))\nhold off\ntitle('Trig Functions')\nlegend('sin(x)', 'cos(x)')\ngrid on" },
      { title: 'Bar chart', description: 'Simple bar chart', code: "clear\nfigure\nbar([4 7 2 9 5 3 8])\ntitle('Weekly Data')\nxlabel('Day')\nylabel('Value')" },
      { title: 'Scatter plot', description: 'Random scatter', code: "clear\nx = rand(1, 30);\ny = rand(1, 30);\nfigure\nscatter(x, y)\ntitle('Random Points')\ngrid on" },
      { title: 'Histogram', description: 'Distribution of random data', code: "clear\ndata = randn(1, 500);\nfigure\nhist(data, 25)\ntitle('Normal Distribution (500 samples)')\nxlabel('Value')\nylabel('Count')" },
      { title: 'Polar pattern', description: 'Antenna array radiation', code: "clear\nN = 8;\nd = 0.5;\ntheta = linspace(0, 2*pi, 720);\nAF = zeros(1, length(theta));\nfor n = 0:N-1\n    AF = AF + exp(1i * 2 * pi * d * n * cos(theta));\nend\nAF = abs(AF) / max(abs(AF));\nfigure\npolarplot(theta, AF)\ntitle('8-element array')\ngrid on" },
      { title: 'Stem plot', description: 'Discrete signal', code: "clear\nn = 0:20;\ny = sin(2 * pi * n / 10);\nfigure\nstem(n, y)\ntitle('Discrete Sinusoid')\nxlabel('Sample n')\nylabel('Amplitude')\ngrid on" },
      { title: 'Stairs plot', description: 'Staircase / zero-order hold', code: "clear\nt = 0:0.5:5;\ny = round(3 * sin(t));\nfigure\nstairs(t, y)\ntitle('Quantized Sine')\nxlabel('Time')\nylabel('Level')\ngrid on" },
      { title: 'Log scale', description: 'semilogy and loglog', code: "clear\nx = 1:10;\ny = 2 .^ x;\nfigure(1)\nsemilogy(x, y)\ntitle('Exponential Growth (semilogy)')\ngrid on\n\nx2 = logspace(0, 3, 50);\ny2 = x2 .^ 2.5;\nfigure(2)\nloglog(x2, y2)\ntitle('Power Law (loglog)')\ngrid on" },
      { title: 'Styled plot', description: 'LineWidth, markers, style strings', code: "clear\nx = linspace(0, 2*pi, 30);\nfigure\nhold on\nplot(x, sin(x), 'b-', 'LineWidth', 2)\nplot(x, sin(x - pi/4), 'r--o', 'LineWidth', 1.5, 'MarkerSize', 4)\nplot(x, sin(x - pi/2), 'g:s', 'MarkerSize', 5)\nhold off\ntitle('Style Strings & Name-Value Pairs')\nlegend('solid', 'dashed+circles', 'dotted+squares')\ngrid on" },
      { title: 'Axis modes', description: 'axis equal, tight', code: "clear\nt = linspace(0, 2*pi, 100);\nfigure(1)\nplot(cos(t), sin(t))\naxis equal\ntitle('Unit Circle — axis equal')\ngrid on\n\nfigure(2)\nx = linspace(0, 10, 50);\nplot(x, sin(x) .* exp(-x/5))\naxis tight\ntitle('Damped Sine — axis tight')\ngrid on" },
      { title: 'Polar config', description: 'thetadir, thetazero, rlim', code: "clear\ntheta = linspace(0, 2*pi, 360);\nrho = abs(cos(2 * theta));\n\nfigure(1)\npolarplot(theta, rho)\ntitle('Default Polar')\ngrid on\n\nfigure(2)\npolarplot(theta, rho)\nthetazero('top')\nthetadir('clockwise')\ntitle('Compass Style')\ngrid on\n\nfigure(3)\npolarplot(theta, rho)\nrlim([0 0.8])\ntitle('rlim [0, 0.8]')\ngrid on" },
    ],
  },
  {
    category: 'Structures', icon: '🏗️',
    items: [
      { title: 'Basic struct', description: 'Create and access struct fields', code: "clear\nperson.name  = 'Alice';\nperson.age   = 30;\nperson.score = [95 87 92];\ndisp(person.name)\ndisp(person.age)\ndisp(person.score)" },
      { title: 'Nested structs', description: 'Structs inside structs', code: "clear\ncar.make = 'Toyota';\ncar.year = 2024;\ncar.engine.horsepower = 203;\ncar.engine.type = 'hybrid';\ndisp(car.make)\ndisp(car.engine.horsepower)" },
    ],
  },
  {
    category: 'Algorithms', icon: '🧮',
    items: [
      { title: 'Bubble Sort', description: 'Sort with swap counter', code: "clear\nfunction result = bubbleSort(arr)\n    n = length(arr);\n    swaps = 0;\n    for i = 1:n-1\n        for j = 1:n-i\n            if arr(j) > arr(j+1)\n                temp = arr(j);\n                arr(j) = arr(j+1);\n                arr(j+1) = temp;\n                swaps = swaps + 1;\n            end\n        end\n    end\n    result.sorted = arr;\n    result.swaps  = swaps;\nend\n\ninfo = bubbleSort([64 34 25 12 22 11 90]);\ndisp(info.sorted)" },
      { title: 'FizzBuzz', description: 'Classic challenge', code: "clear\nfor i = 1:20\n    if mod(i, 15) == 0\n        disp('FizzBuzz')\n    elseif mod(i, 3) == 0\n        disp('Fizz')\n    elseif mod(i, 5) == 0\n        disp('Buzz')\n    else\n        disp(i)\n    end\nend" },
      { title: 'Newton sqrt', description: 'Square root via Newton method', code: "clear\nfunction r = newton_sqrt(x)\n    r = x;\n    for i = 1:20\n        r = (r + x/r) / 2;\n    end\nend\n\ndisp(newton_sqrt(2))\ndisp(newton_sqrt(144))" },
    ],
  },
  {
    category: 'Advanced', icon: '🚀',
    items: [
      { title: 'Complex numbers', description: 'Complex arithmetic', code: "clear\nz1 = 3 + 4i;\nz2 = 1 - 2i;\ndisp(z1 + z2)\ndisp(z1 * z2)\ndisp(abs(z1))\ndisp(real(z1))\ndisp(imag(z1))" },
      { title: 'Matrix operations', description: 'Transpose, reshape, size', code: "clear\nA = [1 2 3; 4 5 6];\ndisp(A')\ndisp(reshape(A, 3, 2))\ndisp(size(A))" },
      { title: 'Statistical analysis', description: 'Mean, variance, std', code: "clear\ndata = [4 8 15 16 23 42];\nn = length(data);\nm = sum(data) / n;\ndisp(m)\n\nv = 0;\nfor i = 1:n\n    v = v + (data(i) - m)^2;\nend\nv = v / (n - 1);\ndisp(v)\ndisp(sqrt(v))" },
      { title: 'Beam steering', description: 'Phased array antenna', code: "clear\nN = 8;\nd = 0.5;\ntheta = linspace(0, 2*pi, 720);\n\nAF = zeros(1, length(theta));\nfor n = 0:N-1\n    AF = AF + exp(1i * 2 * pi * d * n * cos(theta));\nend\nAF = abs(AF) / max(abs(AF));\n\nfigure(1)\npolarplot(theta, AF)\ntitle('Broadside pattern')\ngrid on\n\ntheta0 = pi / 3;\nbeta = -2 * pi * d * cos(theta0);\n\nAF2 = zeros(1, length(theta));\nfor n = 0:N-1\n    AF2 = AF2 + exp(1i * n * (2 * pi * d * cos(theta) + beta));\nend\nAF2 = abs(AF2) / max(abs(AF2));\n\nfigure(2)\npolarplot(theta, AF2)\ntitle('Beam steered to 60 deg')\ngrid on" },
      { title: 'Damped oscillator', description: 'Underdamped response with envelope', code: "clear\nzeta = 0.1;\nwn = 2 * pi;\nwd = wn * sqrt(1 - zeta^2);\n\nt = linspace(0, 5, 500);\ny = exp(-zeta * wn * t) .* cos(wd * t);\nenvelope = exp(-zeta * wn * t);\n\nfigure\nhold on\nplot(t, y, 'b-', 'LineWidth', 1.5)\nplot(t, envelope, 'r--', 'LineWidth', 1)\nplot(t, -envelope, 'r--', 'LineWidth', 1)\nhold off\ntitle('Underdamped Oscillator (zeta = 0.1)')\nxlabel('Time (s)')\nylabel('Amplitude')\nlegend('response', 'envelope')\ngrid on" },
    ],
  },
];

export default EXAMPLES;
