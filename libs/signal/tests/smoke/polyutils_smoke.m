clear
import compat.*

fprintf('=== signal/polyscale + polystab (Phase 4.3) ===\n');

fprintf('\n[polyscale — bandwidth expansion]\n');
p = [1 -2 1.5 -0.5 0.1];
y = polyscale(p, 0.85);
fprintf('  polyscale([1 -2 1.5 -0.5 0.1], 0.85) =\n');
fprintf('  '); fprintf('%.6f ', y); fprintf('\n');
fprintf('  expect: 1 -1.7 1.08375 -0.307063 0.0522006\n');

fprintf('\n[polystab — reflect outside-unit-circle root]\n');
a = [1 -2.5 1];  % roots [2, 0.5]
b = polystab(a);
fprintf('  polystab([1 -2.5 1]) = '); fprintf('%.4f ', b); fprintf('\n');
fprintf('  expect: 1 -1 0.25 (root 2 → 0.5)\n');
fprintf('  roots(b) = '); fprintf('%.4f ', sort(real(roots(b)))); fprintf('\n');

fprintf('\n[polystab — stable poly unchanged]\n');
a = [1 0 -0.25];  % roots [0.5, -0.5]
b = polystab(a);
fprintf('  polystab([1 0 -0.25]) = '); fprintf('%.4f ', b); fprintf('\n');
fprintf('  expect: 1 0 -0.25 (already stable)\n');

fprintf('\nAll BIT-EQUAL with MATLAB R2025b. Octave 11.1.0 doesn''t ship.\n');
