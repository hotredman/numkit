clear
import compat.*
% tan smoke — arrays now run through the SIMD xtan kernel (ported SLEEF:
% Cody-Waite reduction + half-angle polynomial + tan double-angle),
% replacing Div(Sin,Cos). 2-step for |x|<15, extended PI_A..D for |x|<1e6,
% scalar std::tan for |x|>=1e6. ~3.5 ULP, matches MATLAB.
% Run: build/desktop-fast/apps/numkit/Release/numkit.exe \
%      toolboxes/builtin/tests/smoke/tan_simd_smoke.m

fprintf('--- 2-step range (|x|<15) ---\n');
v = tan([0 0.5 1 -1 1.5 2 3 5]);
fprintf('tan([0 .5 1 -1 1.5 2 3 5]):'); fprintf(' %.5g', v); fprintf('\n');
fprintf('  (expect 0 0.5463 1.5574 -1.5574 14.101 -2.185 -0.1425 -3.3805)\n');

fprintf('\n--- extended reduction (15 <= |x| < 1e6) ---\n');
fprintf('tan(10)    = %.15g (expect 0.648360827459087)\n', tan(10));
fprintf('tan(100)   = %.15g (expect -0.587213915156929)\n', tan(100));
fprintf('tan(1000)  = %.15g (expect 1.47032415570272)\n', tan(1000));
fprintf('tan(50000) = %.13g (expect 55.9280569098652)\n', tan(50000));

fprintf('\n--- scalar fallback (|x| >= 1e6) ---\n');
fprintf('tan(2e6) = %.10g (finite, via std::tan)\n', tan(2e6));
