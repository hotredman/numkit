clear

fprintf('=== randsrc (random matrix from finite alphabet) ===\n');

% Default form: alphabet = [-1, 1], uniform
y1 = randsrc(2, 3, [-1 1], 42);
fprintf('  randsrc(2,3, [-1 1], 42):\n');
disp(y1)
fprintf('    Each entry should be -1 or +1.\n');

% Custom alphabet
y2 = randsrc(3, 4, [-3 -1 1 3], 42);
fprintf('  randsrc(3,4, [-3 -1 1 3], 42):\n');
disp(y2)
fprintf('    Each entry should be in {-3, -1, 1, 3}.\n');

% Alphabet with custom probabilities (heavy weight on first symbol)
y3 = randsrc(1, 1000, [1 2 3; 0.7 0.2 0.1], 42);
n1 = sum(y3 == 1);
n2 = sum(y3 == 2);
n3 = sum(y3 == 3);
fprintf('  randsrc(1, 1000, [1 2 3; 0.7 0.2 0.1], 42):\n');
fprintf('    1: %d (~700)  2: %d (~200)  3: %d (~100)  total: %d\n', ...
        n1, n2, n3, n1 + n2 + n3);

% Determinism: same seed → same output
ya = randsrc(5, 5, [-1 1], 7);
yb = randsrc(5, 5, [-1 1], 7);
fprintf('  Deterministic on seed=7: equal = %d (expect 1)\n', isequal(ya, yb));

% Output shape
y_shape = randsrc(3, 7, [0 1], 42);
fprintf('  shape: [%d %d] (expect 3 7)\n', size(y_shape, 1), size(y_shape, 2));
