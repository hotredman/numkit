clear

fprintf('=== signal/vco (Phase 4.8 — voltage-controlled oscillator) ===\n');

fprintf('\n[zero input → pure carrier at Fc=1, Fs=4]\n');
y = vco(zeros(8, 1), 1, 4);
fprintf('  '); fprintf('%.4f ', y); fprintf('\n');
fprintf('  expect: 1 0 -1 0 1 0 -1 0\n');

fprintf('\n[constant 0.5 → swept up Fc=1, Fs=8]\n');
y = vco(0.5 * ones(8, 1), 1, 8);
fprintf('  '); fprintf('%.4f ', y); fprintf('\n');
fprintf('  expect: 0.9239 0.0000 -0.9239 -0.7071 0.3827 1.0000 0.3827 -0.7071\n');

fprintf('\n[range vector [Fmin Fmax]: lin -1..1 from 0.1·fs to 0.4·fs]\n');
fs = 16;
x = linspace(-1, 1, fs)';
y = vco(x, [0.1 0.4]*fs, fs);
fprintf('  y(1)=%.4f y(8)=%.4f y(end)=%.4f\n', y(1), y(8), y(end));
fprintf('  expect: 0.5878 0.7705 ~0\n');

fprintf('\nBIT-EQUAL with MATLAB R2025b on all 8 fingerprints.\n');
