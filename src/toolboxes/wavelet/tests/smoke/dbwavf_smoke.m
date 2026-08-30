clear

% dbwavf — Daubechies scaling filter (Lo_R / sqrt(2), sum = 1).

fprintf('=== dbwavf("db1") ===\n');
disp(dbwavf('db1'));
fprintf('  expect: [0.5 0.5]\n\n');

fprintf('=== dbwavf("db2") ===\n');
disp(dbwavf('db2'));
fprintf('  expect: [0.341506 0.591506 0.158494 -0.091506]\n\n');

fprintf('=== dbwavf("db4") ===\n');
disp(dbwavf('db4'));
fprintf('  expect: [0.162902 0.505473 0.446100 -0.019788 -0.132253 0.021808 0.023251 -0.007493]\n\n');

fprintf('=== sum normalised to 1 ===\n');
fprintf('  sum(db4) = %.10f (expect 1)\n', sum(dbwavf('db4')));
