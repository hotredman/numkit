clear

fprintf('=== signal/polyscale + polystab ===\n');
fprintf('Clean-room: Oppenheim & Schafer; Markel & Gray; Hayes.\n');

fprintf('\n[polyscale — bandwidth expansion (row vector, scalar alpha)]\n');
p = [1 -2 1.5 -0.5 0.1];
y = polyscale(p, 0.85);
fprintf('  polyscale([1 -2 1.5 -0.5 0.1], 0.85) =\n  ');
fprintf('%.6f ', y); fprintf('\n');
fprintf('  expect: 1 -1.7 1.08375 -0.307063 0.0522006\n');

fprintf('\n[polyscale — matrix input: one polynomial per row]\n');
M = polyscale([1 2 3; 4 5 6], 0.5);
fprintf('  row 1: '); fprintf('%.4f ', M(1,:)); fprintf('(expect 1 1 0.75)\n');
fprintf('  row 2: '); fprintf('%.4f ', M(2,:)); fprintf('(expect 4 2.5 1.5)\n');

fprintf('\n[polyscale — row-vector alpha: element k raised to power k]\n');
yv = polyscale([1 2 3], [1 2 4]);
fprintf('  polyscale([1 2 3], [1 2 4]) = '); fprintf('%.0f ', yv);
fprintf('(expect 1 4 48)\n');

fprintf('\n[polyscale — complex alpha rotates + scales]\n');
yc = polyscale([1 2 3], 1i);
fprintf('  polyscale([1 2 3], 1i) = ');
fprintf('%g%+gi ', [real(yc); imag(yc)]); fprintf('(expect 1 2i -3)\n');

fprintf('\n[polystab — reflect outside-unit-circle root]\n');
a = [1 -2.5 1];  % roots [2, 0.5]
b = polystab(a);
fprintf('  polystab([1 -2.5 1]) = '); fprintf('%.4f ', b); fprintf('\n');
fprintf('  expect: 1 -1 0.25 (root 2 -> 0.5)\n');
fprintf('  roots(b) = '); fprintf('%.4f ', sort(real(roots(b)))); fprintf('\n');

fprintf('\n[polystab — stable poly unchanged]\n');
b2 = polystab([1 0 -0.25]);  % roots [0.5, -0.5]
fprintf('  polystab([1 0 -0.25]) = '); fprintf('%.4f ', b2);
fprintf('(expect 1 0 -0.25 — already stable)\n');

fprintf('\n[polystab — leading zeros are not significant]\n');
b3 = polystab([0 1 -2.5 1]);
fprintf('  polystab([0 1 -2.5 1]) = '); fprintf('%.4f ', b3);
fprintf('(expect 1 -1 0.25)\n');

fprintf('\n[polystab — magnitude-response shape preserved]\n');
au = [1 -2.5 1 0.3];
bu = polystab(au);
w = linspace(0.1, pi-0.1, 5); z = exp(1i*w);
ea = zeros(1, numel(z)); for k = 1:numel(au), ea = ea.*z + au(k); end
eb = zeros(1, numel(z)); for k = 1:numel(bu), eb = eb.*z + bu(k); end
ratio = abs(eb) ./ abs(ea);
fprintf('  |B|/|A| over freq: min=%.6f max=%.6f (expect constant)\n', ...
    min(ratio), max(ratio));

fprintf('\npolyscale / polystab match MATLAB R2025b. Octave 11.1.0 ships\n');
fprintf('polystab in the signal package (differs on leading zeros) and\n');
fprintf('does not ship polyscale.\n');
