const CHEAT_SHEET = [
  {
    title: 'Operators',
    items: [
      { code: '+ - * /', desc: 'Arithmetic' },
      { code: '.* ./ .^', desc: 'Element-wise ops' },
      { code: '^', desc: 'Power / matrix power' },
      { code: '== ~= < > <= >=', desc: 'Comparison' },
      { code: '& | ~ &&  ||', desc: 'Logical' },
      { code: ':', desc: 'Range / slicing' },
      { code: "'", desc: 'Transpose' },
    ],
  },
  {
    title: 'Matrix Creation',
    items: [
      { code: '[1 2 3; 4 5 6]', desc: 'Manual matrix' },
      { code: 'zeros(m,n)', desc: 'Zero matrix' },
      { code: 'ones(m,n)', desc: 'Ones matrix' },
      { code: 'eye(n)', desc: 'Identity matrix' },
      { code: 'rand(m,n)', desc: 'Random [0,1)' },
      { code: 'linspace(a,b,n)', desc: 'Linear spacing' },
      { code: 'a:step:b', desc: 'Range with step' },
    ],
  },
  {
    title: 'Indexing',
    items: [
      { code: 'A(i,j)', desc: 'Element access' },
      { code: 'A(i,:)', desc: 'Entire row i' },
      { code: 'A(:,j)', desc: 'Entire column j' },
      { code: 'A(2:4, 1:3)', desc: 'Submatrix' },
      { code: 'A(end)', desc: 'Last element' },
    ],
  },
  {
    title: 'Control Flow',
    items: [
      { code: 'if / elseif / else / end', desc: 'Conditional' },
      { code: 'for i = 1:n ... end', desc: 'For loop' },
      { code: 'while cond ... end', desc: 'While loop' },
      { code: 'switch / case / end', desc: 'Switch' },
      { code: 'try / catch / end', desc: 'Error handling' },
      { code: 'break / continue', desc: 'Loop control' },
    ],
  },
  {
    title: 'Functions',
    items: [
      { code: 'function y = f(x)', desc: 'Single output' },
      { code: 'function [a,b] = f(x)', desc: 'Multiple outputs' },
      { code: 'return', desc: 'Early return' },
    ],
  },
  {
    title: 'Keyboard Shortcuts',
    items: [
      { code: 'Enter', desc: 'Execute command' },
      { code: 'Shift+Enter', desc: 'New line' },
      { code: 'Tab', desc: 'Autocomplete' },
      { code: '↑ / ↓', desc: 'History navigation' },
      { code: 'Ctrl+L', desc: 'Clear screen' },
      { code: 'Ctrl+C', desc: 'Cancel input' },
    ],
  },
];

export default CHEAT_SHEET;
