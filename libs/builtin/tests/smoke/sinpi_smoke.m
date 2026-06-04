clear
import compat.*
% sinpi / cospi smoke — accurate sin(pi*x) / cos(pi*x).
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      libs/builtin/tests/smoke/sinpi_smoke.m

fprintf('--- exact zeros at integers / half-integers ---\n');
fprintf('sinpi(1)   = %.3e   (expect exactly 0)\n', sinpi(1));
fprintf('sinpi(1e7) = %.3e   (expect 0; naive sin(pi*x) gave ~5.6e-10)\n', sinpi(1e7));
fprintf('cospi(0.5) = %.3e   (expect exactly 0)\n', cospi(0.5));

fprintf('\n--- sign of the zero matches MATLAB ---\n');
fprintf('sinpi(1)  signbit=%d (expect 0, +0)\n', 1/sinpi(1)  < 0);
fprintf('sinpi(-1) signbit=%d (expect 1, -0)\n', 1/sinpi(-1) < 0);
fprintf('cospi(.5) signbit=%d (expect 0, +0)\n', 1/cospi(0.5) < 0);

fprintf('\n--- accurate values (closed form) ---\n');
fprintf('sinpi(1/6) = %.17g (expect 0.5)\n', sinpi(1/6));
fprintf('sinpi(1/3) = %.17g (expect ~0.86602540378443860)\n', sinpi(1/3));
fprintf('cospi(1/3) = %.17g (expect 0.5)\n', cospi(1/3));
fprintf('cospi(1/6) = %.17g (expect ~0.86602540378443860)\n', cospi(1/6));

fprintf('\n--- large arguments stay correct ---\n');
fprintf('sinpi(1e10+0.5) = %.17g (expect 1)\n', sinpi(1e10+0.5));
fprintf('sinpi(123456.25) = %.17g (expect ~0.70710678118654757)\n', sinpi(123456.25));

fprintf('\n--- specials ---\n');
fprintf('sinpi(Inf) = %g (expect NaN)\n', sinpi(Inf));
fprintf('cospi(NaN) = %g (expect NaN)\n', cospi(NaN));

fprintf('\n--- vector (SIMD body + tail) ---\n');
v = sinpi((0:9)/10);
fprintf('sinpi(0:9/10):'); fprintf(' %.4f', v); fprintf('\n');
fprintf('  (expect 0  0.3090  0.5878  0.8090  0.9511  1  0.9511  0.8090  0.5878  0.3090)\n');
