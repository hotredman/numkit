clear

% hilbert: analytic signal with MATLAB-matching sign convention.
% real(h) = x, imag(h) = +H{x} (positive frequencies multiplied by +i).

x = (1:8)';
h = hilbert(x);
fprintf('=== hilbert([1:8]'') ===\n');
fprintf('  real: '); disp(real(h)');
fprintf('  expect: [1 2 3 4 5 6 7 8]\n');
fprintf('  imag: '); disp(imag(h)');
fprintf('  expect: [3.8284 -1 -1 -1.8284 -1.8284 -1 -1 3.8284]\n\n');

fprintf('=== envelope (sign-invariant magnitude) ===\n');
disp(envelope(x)');
