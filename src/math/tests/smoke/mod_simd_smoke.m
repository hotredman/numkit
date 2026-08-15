clear
import compat.*
% mod smoke — real-double arrays now take the Highway SIMD path
% (a - floor(a/b)*b, bit-identical to the scalar formula). Integer-typed
% operands and broadcasting still use the scalar reference. rem unchanged.
% Run: build/desktop-fast/apps/numkit/Release/numkit.exe \
%      toolboxes/builtin/tests/smoke/mod_simd_smoke.m

fprintf('--- same-shape vector (VV) ---\n');
v = mod([5 -5 7 -7 8 -8 9 -9 100 -100], 3);
fprintf('mod(...,3):'); fprintf(' %g', v); fprintf('\n');
fprintf('  (expect 2 1 1 2 2 1 0 0 1 2)\n');

fprintf('\n--- array-by-scalar (VS) ---\n');
fprintf('mod(1:10, 3):'); fprintf(' %g', mod(1:10, 3)); fprintf('\n');
fprintf('  (expect 1 2 0 1 2 0 1 2 0 1)\n');

fprintf('\n--- scalar-by-array (SV) ---\n');
fprintf('mod(10, [3 4 7 6]):'); fprintf(' %g', mod(10, [3 4 7 6])); fprintf('\n');
fprintf('  (expect 1 2 3 4)\n');

fprintf('\n--- sign follows the divisor; fractional ---\n');
fprintf('mod([5 -5 5 -5],[3 3 -3 -3]):'); fprintf(' %g', mod([5 -5 5 -5],[3 3 -3 -3])); fprintf('\n');
fprintf('  (expect 2 1 -1 -2)\n');
fprintf('mod([5.5 -5.5 7.25],3):'); fprintf(' %g', mod([5.5 -5.5 7.25],3)); fprintf('\n');
fprintf('  (expect 2.5 0.5 1.25)\n');

fprintf('\n--- zero divisor in an array -> dividend ---\n');
fprintf('mod([5 6 7 8],[0 3 0 4]):'); fprintf(' %g', mod([5 6 7 8],[0 3 0 4])); fprintf('\n');
fprintf('  (expect 5 0 7 0)\n');

fprintf('\n--- scalar mod(a,0) == a (MATLAB; was NaN via fmod fast path) ---\n');
fprintf('mod(5,0)=%g mod(-5,0)=%g mod(0,0)=%g mod(5.5,0)=%g (expect 5 -5 0 5.5)\n', ...
        mod(5,0), mod(-5,0), mod(0,0), mod(5.5,0));
fprintf('rem(5,0)=%g (expect NaN, unchanged)\n', rem(5,0));
