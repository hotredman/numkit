clear

import compat.*

% --- Round-trip: scramble + descramble recovers the input bit sequence ---
% Use V.34-like polynomial 1 + z^-18 + z^-23 (small example: simpler 5-bit poly)
% Polynomial 1 + z^-2 + z^-5 → poly = [1 0 1 0 0 1]
poly = [1 0 1 0 0 1];
init = [0 0 0 0 0];

rng(42);
x = double(rand(64, 1) > 0.5);   % random bits
y = scrambler(x, poly, init);
xr = descrambler(y, poly, init);

fprintf('--- scrambler/descrambler round-trip ---\n');
fprintf('  numel(x) = %d\n', numel(x));
fprintf('  max|x - xr| = %.6e (expect 0 — exact recovery)\n\n', ...
    max(abs(x - xr)));

% --- All-zero input scrambled with non-zero state produces non-trivial output ---
x0 = zeros(20, 1);
init1 = [1 0 0 0 0];
y0 = scrambler(x0, poly, init1);
fprintf('--- scrambler(zeros, init=[1 0 0 0 0]) ---\n');
fprintf('  count(y0=1) = %d (expect > 0 — register state seeds output)\n\n', ...
    sum(y0));

% --- All-zero input + all-zero state → all-zero output ---
y_zero = scrambler(zeros(10, 1), poly, [0 0 0 0 0]);
fprintf('--- scrambler(zeros, zero-state) ---\n');
fprintf('  sum(|y|) = %d (expect 0 — pure pass-through)\n\n', sum(abs(y_zero)));

% --- Self-synchronizing: descrambler with WRONG initState still
%     synchronizes after order-many bits ---
x_test = double(rand(40, 1) > 0.5);
y_test = scrambler(x_test, poly, init);
% Descrambler with a different initial state.
xr_wrong = descrambler(y_test, poly, [1 1 0 0 1]);
% First n=5 bits will be wrong, after that they should match.
mismatch = sum(x_test ~= xr_wrong);
fprintf('--- self-sync: descrambler with WRONG init ---\n');
fprintf('  total mismatches = %d (expect ≤ 5 — register length)\n', mismatch);
% Verify tail is correctly synced:
tail_err = sum(x_test(6:end) ~= xr_wrong(6:end));
fprintf('  tail mismatches (after order=5) = %d (expect 0)\n\n', tail_err);

% --- Different polynomials produce different outputs ---
poly_a = [1 0 1 0 0 1];
poly_b = [1 1 0 0 0 1];
x = double(rand(20, 1) > 0.5);
ya = scrambler(x, poly_a, init);
yb = scrambler(x, poly_b, init);
fprintf('--- different polynomials, same input ---\n');
fprintf('  count(ya != yb) = %d (expect > 0 — different LFSR ⇒ different scrambling)\n', ...
    sum(ya ~= yb));
