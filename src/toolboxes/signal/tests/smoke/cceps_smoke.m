clear

% cceps — complex cepstrum: ifft(log(fft(x))) with phase unwrapping.
%
% numkit historically time-reversed
% the output because the second-pass fftRadix2 was called with the
% wrong sign argument. After the fix, output is bit-identical to
% MATLAB R2025b on the canonical probe.

x = (1:8)';
y = cceps(x);
fprintf('=== cceps((1:8)'') ===\n');
fprintf('  y = '); disp(y');
fprintf('  expect MATLAB:\n');
fprintf('   2.008  -0.0436  -0.00834  0.0375  0.1014  0.2002  0.3844  0.9045\n\n');

fprintf('=== icceps round-trip ===\n');
c = cceps(x);
xr = icceps(c);
fprintf('  xr = '); disp(xr');
fprintf('  expect (per MATLAB; circular-shift of x): 8 1 2 3 4 5 6 7\n');
fprintf('  numel = %d, max = %g, min = %g, sum = %g\n', ...
        numel(xr), max(xr), min(xr), sum(xr));
fprintf('  expect: 8, 8, 1, 36\n');
