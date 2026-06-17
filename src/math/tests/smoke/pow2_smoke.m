clear
import compat.*
% pow2 smoke — 2^x now SIMD-backed (ported SLEEF xexp2: integer-exact
% reduction + single-double polynomial). 2-arg pow2(f,e)=f*2^e unchanged.
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      toolboxes/builtin/tests/smoke/pow2_smoke.m

fprintf('--- pow2 vector (SIMD body + tail): integer exponents exact ---\n');
p = pow2([0 1 2 3 4 5 6 7 8 30]);
fprintf('pow2(0..8,30):'); fprintf(' %g', p); fprintf('\n');
fprintf('  (expect 1 2 4 8 16 32 64 128 256 1073741824)\n');

fprintf('\n--- fractional exponents ---\n');
fprintf('pow2(0.5)  = %.17g (expect ~1.4142135623730951)\n', pow2(0.5));
fprintf('pow2(-3)   = %g (expect 0.125)\n', pow2(-3));
fprintf('pow2(10.5) = %.17g (expect ~1448.1546878700492)\n', pow2(10.5));

fprintf('\n--- overflow / underflow edges ---\n');
fprintf('pow2(1100)  = %g (expect Inf)\n', pow2(1100));
fprintf('pow2(-2100) = %g (expect 0)\n', pow2(-2100));

fprintf('\n--- 2-arg pow2(f,e) = f*2^e (unchanged) ---\n');
fprintf('pow2(1.5, 2) = %g (expect 6)\n', pow2(1.5, 2));
fprintf('pow2(1, 10)  = %g (expect 1024)\n', pow2(1, 10));
