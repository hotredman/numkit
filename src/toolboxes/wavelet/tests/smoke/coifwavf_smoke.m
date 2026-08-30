clear

% coifwavf — Coiflet scaling filter (Lo_R / sqrt(2), sum = 1).

fprintf('=== coifwavf("coif1") ===\n');
disp(coifwavf('coif1'));
fprintf('  expect: [-0.051430 0.238930 0.602859 0.272140 -0.051430 -0.011070]\n');
fprintf('  sum    = %.10f (expect 1)\n', sum(coifwavf('coif1')));
