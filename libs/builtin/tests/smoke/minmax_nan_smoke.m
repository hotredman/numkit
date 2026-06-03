clear
import compat.*
% min/max two-argument NaN handling — MATLAB ignores NaN (returns the
% non-NaN operand; NaN only if both are NaN). numkit previously returned
% NaN: the VM scalar fast path used (a>=b)?a:b, the Value-level used
% std::max/std::min (NaN-order-dependent). Fixed to std::fmax/std::fmin.
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      libs/builtin/tests/smoke/minmax_nan_smoke.m

fprintf('--- scalar 2-arg (fast path) ---\n');
fprintf('max(5,NaN)=%g max(NaN,5)=%g min(5,NaN)=%g min(NaN,5)=%g (expect 5 5 5 5)\n', ...
        max(5,NaN), max(NaN,5), min(5,NaN), min(NaN,5));
fprintf('max(NaN,NaN)=%g min(NaN,NaN)=%g (expect NaN NaN)\n', max(NaN,NaN), min(NaN,NaN));

fprintf('\n--- array 2-arg (Value-level), NaN in either position ---\n');
fprintf('max([NaN 6 8],[5 7 NaN]): '); fprintf('%g ', max([NaN 6 8],[5 7 NaN])); fprintf('(expect 5 7 8)\n');
fprintf('min([NaN 6 8],[5 7 NaN]): '); fprintf('%g ', min([NaN 6 8],[5 7 NaN])); fprintf('(expect 5 6 8)\n');

fprintf('\n--- normal cases unchanged ---\n');
fprintf('max(3,7)=%g min(3,7)=%g max(-Inf,5)=%g min(Inf,5)=%g (expect 7 3 5 5)\n', ...
        max(3,7), min(3,7), max(-Inf,5), min(Inf,5));
