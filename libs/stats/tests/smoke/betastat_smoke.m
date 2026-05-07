clear

import compat.*

fprintf('=== betastat ===\n');

[m, v] = betastat(2, 3);
fprintf('  Beta(2,3): m=%.4f v=%.4f (expect 0.4000 / 0.0400)\n', m, v);

[m, v] = betastat(1, 1);
fprintf('  Beta(1,1): m=%.4f v=%.6f (expect 0.5000 / 0.083333) [uniform]\n', m, v);

% Vector inputs — MATLAB-style broadcasting
[m, v] = betastat([0.5 1 2 5 10], [0.5 1 5 5 10]);
fprintf('\n  vector m = [%.4f %.4f %.4f %.4f %.4f]\n', m(1), m(2), m(3), m(4), m(5));
fprintf('  expect:    [0.5000 0.5000 0.2857 0.5000 0.5000]\n');
fprintf('  vector v = [%.4f %.4f %.4f %.4f %.4f]\n', v(1), v(2), v(3), v(4), v(5));
fprintf('  expect:    [0.1250 0.0833 0.0255 0.0227 0.0119]\n');

% Scalar broadcast with vector
[m, v] = betastat(2, [1 2 3]);
fprintf('\n  scalar+vec m = [%.4f %.4f %.4f] (expect [0.6667 0.5000 0.4000])\n', m(1), m(2), m(3));

fprintf('\n--- invalid params ---\n');
fprintf('  a=0  : m=%g v=%g (expect NaN NaN)\n', betastat(0, 3));
fprintf('  b<0  : m=%g (expect NaN)\n', betastat(2, -1));
