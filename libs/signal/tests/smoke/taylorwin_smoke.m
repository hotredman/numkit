clear

import compat.*

fprintf('=== taylorwin ===\n');
fprintf('  Bug fix 2026-05-08: previous impl was inverted (peak at\n');
fprintf('  edges, dip at center) and wrongly normalised peak to 1.\n');
fprintf('  MATLAB does NOT normalise — natural amplitude depends on\n');
fprintf('  (nbar, sll); for default (4, -30) peak ≈ 1.52.\n\n');

fprintf('  taylorwin(8) (default 4, -30):\n   '); fprintf('%.4f ', taylorwin(8)); fprintf('\n');
fprintf('  expect: 0.4353 0.8024 1.2423 1.5201 1.5201 1.2423 0.8024 0.4353\n');

fprintf('  taylorwin(8, 4, -40):\n   '); fprintf('%.4f ', taylorwin(8, 4, -40)); fprintf('\n');
fprintf('  expect: 0.2794 0.7299 1.2984 1.6923 1.6923 1.2984 0.7299 0.2794\n');

fprintf('  taylorwin(8, 6, -30):\n   '); fprintf('%.4f ', taylorwin(8, 6, -30)); fprintf('\n');
fprintf('  expect: 0.4488 0.8042 1.2360 1.5110 1.5110 1.2360 0.8042 0.4488\n');

w64 = taylorwin(64, 8, -60);
fprintf('  taylorwin(64, 8, -60) edge=%.6f center=%.4f\n', w64(1), w64(32));

fprintf('  taylorwin(1) = %.4f (expect ≈ 1.5581 — formula at N=1)\n', taylorwin(1));
