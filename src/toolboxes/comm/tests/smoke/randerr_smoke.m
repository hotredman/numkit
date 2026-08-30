clear

fprintf('=== randerr (random binary error matrix) ===\n');

% Default: 1 error per row
y1 = randerr(5, 8, 1, 42);
fprintf('  randerr(5,8,1, 42):\n');
disp(y1)
fprintf('  row sums: ');
fprintf('%d ', sum(y1, 2));
fprintf(' (expect all 1)\n');

% 3 errors per row
y2 = randerr(3, 10, 3, 42);
fprintf('  randerr(3,10,3, 42):\n');
disp(y2)
fprintf('  row sums: ');
fprintf('%d ', sum(y2, 2));
fprintf(' (expect all 3)\n');

% Variable errors per row [1 2 3] uniform
y3 = randerr(5, 10, [1 2 3], 42);
fprintf('  randerr(5,10, [1 2 3], 42):\n');
disp(y3)
fprintf('  row sums: ');
fprintf('%d ', sum(y3, 2));
fprintf('\n');

% Weighted: prefer 1 error
y4 = randerr(5, 10, [0 1 2; 0.0 0.7 0.3], 42);
fprintf('  randerr(5,10, [0 1 2; 0.0 0.7 0.3], 42):\n');
disp(y4)
fprintf('  row sums: ');
fprintf('%d ', sum(y4, 2));
fprintf('\n');

% Determinism check
ya = randerr(10, 10, 2, 7);
yb = randerr(10, 10, 2, 7);
fprintf('  Deterministic on seed=7: equal = %d (expect 1)\n', isequal(ya, yb));
