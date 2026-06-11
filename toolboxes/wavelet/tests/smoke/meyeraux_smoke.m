clear

import compat.*

fprintf('=== meyeraux ===\n');
% On-support (MATLAB polynomial)
fprintf('  vector: '); fprintf('%.4f ', meyeraux([0 0.25 0.5 0.75 1])); fprintf('\n');
fprintf('  expect: 0 0.0706 0.5 0.9294 1\n');

% Bug fix 2026-05-08: clipping outside [0, 1]
fprintf('  meyeraux(-0.5) = %g (expect 0 — clip; was 6.0625)\n', meyeraux(-0.5));
fprintf('  meyeraux(2)    = %g (expect 1 — clip; was -208)\n', meyeraux(2));

% Mixed vector with out-of-support values
fprintf('  mixed: '); fprintf('%.4f ', meyeraux([-1 0 0.5 1 2])); fprintf('\n');
fprintf('  expect: 0 0 0.5 1 1\n');
