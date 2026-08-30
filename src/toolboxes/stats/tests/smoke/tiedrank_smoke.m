clear

fprintf('=== tiedrank (ranks adjusted for ties) ===\n');

[r, t] = tiedrank([10 20 30 20 10 40]);
fprintf('  vector [10 20 30 20 10 40]:\n');
fprintf('    r = '); fprintf('%g ', r); fprintf('  (expect 1.5 3.5 5 3.5 1.5 6)\n');
fprintf('    tieadj = %g (expect 6)\n', t);

[r2, t2] = tiedrank([5 5 5 5]);
fprintf('  all-equal [5 5 5 5]:\n');
fprintf('    r = '); fprintf('%g ', r2); fprintf('  (expect 2.5 2.5 2.5 2.5)\n');
fprintf('    tieadj = %g (expect 30)\n', t2);

[r3, t3] = tiedrank([1 NaN 3 2]);
fprintf('  with NaN [1 NaN 3 2]:\n');
fprintf('    r = '); fprintf('%g ', r3); fprintf('  (expect 1 NaN 3 2)\n');
fprintf('    tieadj = %g (expect 0)\n', t3);

[rm, tm] = tiedrank([3 1; 5 2; 5 1; 1 4]);
fprintf('  matrix tiedrank — column-wise:\n');
disp(rm)
fprintf('    expected:\n      2.0  1.5\n      3.5  3.0\n      3.5  1.5\n      1.0  4.0\n');
fprintf('    tieadj = '); fprintf('%g ', tm); fprintf('  (expect 3 3)\n');
