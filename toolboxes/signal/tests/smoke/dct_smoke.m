clear

import compat.*

fprintf('=== dct / idct ===\n');

% Vector
y = dct((1:8)');
fprintf('  dct((1:8))      : '); fprintf('%.4f ', y); fprintf('\n');
fprintf('  expect: 12.7279 -6.4423 0 -0.6735 0 -0.2009 0 -0.0507\n');

% Matrix per-column (bug fix 2026-05-08: was flat-vector)
M = [1 5; 2 6; 3 7; 4 8];
yM = dct(M);
fprintf('  dct(M) col 1    : '); fprintf('%.4f ', yM(:,1)); fprintf('\n');
fprintf('  expect:           5 -2.2304 0 -0.1585\n');
fprintf('  dct(M) col 2    : '); fprintf('%.4f ', yM(:,2)); fprintf('\n');
fprintf('  expect:           13 -2.2304 0 -0.1585\n');

% Length override
y6 = dct((1:8)', 6);
fprintf('  dct(x, 6)       : '); fprintf('%.4f ', y6); fprintf('\n');
y10 = dct((1:8)', 10);
fprintf('  dct(x, 10)      : '); fprintf('%.4f ', y10); fprintf('\n');

% dim arg
yM2 = dct(M, 4, 2);
fprintf('  dct(M, 4, 2) row-wise:\n');
for i = 1:size(yM2, 1)
    fprintf('    '); fprintf('%.4f ', yM2(i,:)); fprintf('\n');
end

% Round-trip
xR = idct(dct((1:8)'));
fprintf('  idct(dct(1:8)) max err: %g\n', max(abs(xR - (1:8)')));

% 'Type' explicit error
try
    dct((1:8)', 'Type', 1);
catch err
    fprintf('  dct(Type, 1) → %s\n', err.message);
end
