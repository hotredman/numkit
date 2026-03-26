const EXAMPLES = [
  {
    category: 'Basics', icon: '📐',
    items: [
      { title: 'Arithmetic', description: 'Basic math operations', code: 'disp(2 + 3 * 4 - 1)\ndisp((2 + 3) * (4 - 1))\ndisp(2 ^ 3 ^ 2)\ndisp(mod(17, 5))' },
      { title: 'Variables', description: 'Assign and use variables', code: 'x = 42;\ny = 3.14;\nresult = x * y;\ndisp(result)' },
      { title: 'Math functions', description: 'sqrt, abs, log, exp, trig', code: 'disp(sqrt(144))\ndisp(abs(-7))\ndisp(log(exp(5)))\ndisp(floor(3.7))\ndisp(ceil(-2.3))' },
      { title: 'Strings', description: 'String operations', code: "disp(upper('hello'))\ndisp(lower('MATLAB'))\ndisp(length('OpenAI'))\ndisp(strcmp('test', 'test'))" },
    ],
  },
  {
    category: 'Matrices', icon: '🔢',
    items: [
      { title: 'Create & index', description: 'Matrix creation and element access', code: 'A = [1 2 3; 4 5 6; 7 8 9];\ndisp(A)\ndisp(A(2, 3))' },
      { title: 'Matrix arithmetic', description: 'Addition and element-wise multiply', code: 'A = [1 2 3; 4 5 6; 7 8 9];\nB = [9 8 7; 6 5 4; 3 2 1];\ndisp(A + B)\ndisp(A .* B)' },
      { title: 'Special matrices', description: 'eye, zeros, ones, linspace', code: 'disp(eye(3))\ndisp(zeros(2))\ndisp(ones(1, 4))\ndisp(linspace(0, 1, 5))' },
      { title: 'Vector operations', description: 'Ranges, sum, min, max', code: 'v = 1:10;\ndisp(v)\ndisp(sum(v))\ndisp(min(v))\ndisp(max(v))' },
    ],
  },
  {
    category: 'Control Flow', icon: '🔄',
    items: [
      { title: 'For loop', description: 'Sum of 1 to 100', code: 'total = 0;\nfor i = 1:100\n    total = total + i;\nend\ndisp(total)' },
      { title: 'While loop', description: 'First power of 2 >= 1000', code: 'n = 1;\nwhile n < 1000\n    n = n * 2;\nend\ndisp(n)' },
      { title: 'If / elseif / else', description: 'Conditional branching', code: "val = 42;\nif val > 100\n    disp('big')\nelseif val > 10\n    disp('medium')\nelse\n    disp('small')\nend" },
      { title: 'Nested loops', description: 'Multiplication table', code: "for i = 1:5\n    row = '';\n    for j = 1:5\n        row = [row, num2str(i*j), ' '];\n    end\n    disp(row)\nend" },
    ],
  },
  {
    category: 'Functions', icon: '⚡',
    items: [
      { title: 'Factorial', description: 'Recursive factorial function', code: 'function y = factorial(n)\n    if n <= 1\n        y = 1;\n    else\n        y = n * factorial(n - 1);\n    end\nend\n\ndisp(factorial(10))' },
      { title: 'Fibonacci', description: 'Recursive Fibonacci sequence', code: 'function r = fib(n)\n    if n <= 1\n        r = n;\n    else\n        r = fib(n-1) + fib(n-2);\n    end\nend\n\nresult = [];\nfor k = 0:9\n    result = [result, fib(k)];\nend\ndisp(result)' },
      { title: 'Multiple returns', description: 'Function returning min and max', code: "function [mn, mx] = minmax(v)\n    mn = v(1); mx = v(1);\n    for i = 2:length(v)\n        if v(i) < mn\n            mn = v(i);\n        end\n        if v(i) > mx\n            mx = v(i);\n        end\n    end\nend\n\n[lo, hi] = minmax([5 3 9 1 7]);\ndisp(lo)\ndisp(hi)" },
    ],
  },
  {
    category: 'Plotting', icon: '📊',
    items: [
      { title: 'Sine wave', description: 'Basic line plot', code: "x = linspace(0, 2*pi, 100);\ny = sin(x);\nplot(x, y)\ntitle('Sine Wave')\nxlabel('x')\nylabel('sin(x)')" },
      { title: 'Multiple curves', description: 'Plot sin & cos together', code: "x = linspace(0, 2*pi, 100);\nplot(x, sin(x))\nplot(x, cos(x))\ntitle('Trig Functions')" },
      { title: 'Bar chart', description: 'Simple bar chart', code: "bar([4 7 2 9 5 3 8])\ntitle('Weekly Data')" },
      { title: 'Scatter plot', description: 'Random scatter', code: "x = rand(1, 30);\ny = rand(1, 30);\nscatter(x, y)\ntitle('Random Points')" },
    ],
  },
  {
    category: 'Structures', icon: '🏗️',
    items: [
      { title: 'Basic struct', description: 'Create and access struct fields', code: "person.name  = 'Alice';\nperson.age   = 30;\nperson.score = [95 87 92];\ndisp(person.name)\ndisp(person.age)" },
      { title: 'Nested structs', description: 'Structs inside structs', code: "car.make = 'Toyota';\ncar.year = 2024;\ncar.engine.horsepower = 203;\ncar.engine.type = 'hybrid';\ndisp(car.make)\ndisp(car.engine.horsepower)" },
    ],
  },
  {
    category: 'Algorithms', icon: '🧮',
    items: [
      { title: 'Bubble Sort', description: 'Sort with swap counter', code: "function result = bubbleSort(arr)\n    n = length(arr);\n    swaps = 0;\n    for i = 1:n-1\n        for j = 1:n-i\n            if arr(j) > arr(j+1)\n                temp = arr(j);\n                arr(j) = arr(j+1);\n                arr(j+1) = temp;\n                swaps = swaps + 1;\n            end\n        end\n    end\n    result.sorted = arr;\n    result.swaps  = swaps;\nend\n\ninfo = bubbleSort([64 34 25 12 22 11 90]);\ndisp(info.sorted)" },
      { title: 'FizzBuzz', description: 'Classic challenge', code: "for i = 1:20\n    if mod(i, 15) == 0\n        disp('FizzBuzz')\n    elseif mod(i, 3) == 0\n        disp('Fizz')\n    elseif mod(i, 5) == 0\n        disp('Buzz')\n    else\n        disp(i)\n    end\nend" },
      { title: 'Newton sqrt', description: 'Square root via Newton method', code: "function r = newton_sqrt(x)\n    r = x;\n    for i = 1:20\n        r = (r + x/r) / 2;\n    end\nend\n\ndisp(newton_sqrt(2))\ndisp(newton_sqrt(144))" },
    ],
  },
  {
    category: 'Advanced', icon: '🚀',
    items: [
      { title: 'Matrix operations', description: 'Transpose, trace, reshape', code: "A = [1 2 3; 4 5 6];\ndisp(A')\ndisp(reshape(A, 3, 2))\ndisp(size(A))" },
      { title: 'Complex numbers', description: 'Complex arithmetic', code: 'z1 = 3 + 4i;\nz2 = 1 - 2i;\ndisp(abs(z1))\ndisp(real(z1))\ndisp(imag(z1))' },
      { title: 'Statistical analysis', description: 'Mean, variance, std', code: "data = [4 8 15 16 23 42];\nn = length(data);\nm = sum(data) / n;\ndisp(m)\n\nv = 0;\nfor i = 1:n\n    v = v + (data(i) - m)^2;\nend\nv = v / (n - 1);\ndisp(v)\ndisp(sqrt(v))" },
    ],
  },
];

export default EXAMPLES;
