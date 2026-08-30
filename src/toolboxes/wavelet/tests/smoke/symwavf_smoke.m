clear

fprintf('=== symwavf("sym2") ===\n');
disp(symwavf('sym2'));
fprintf('  expect: [0.3415 0.5915 0.1585 -0.0915]\n\n');

fprintf('=== symwavf("sym4") ===\n');
disp(symwavf('sym4'));
fprintf('  expect: [0.0228 -0.0089 -0.0702 0.2106 0.5683 0.3519 -0.0210 -0.0536]\n');
fprintf('  sum    = %.10f (expect 1)\n', sum(symwavf('sym4')));
