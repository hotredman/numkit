clear
import compat.*
% power complex-promotion smoke — a negative real base ^ non-integer
% exponent is complex (MATLAB); integer exponents / positive bases stay
% real; arrays promote per-pair. numkit previously returned real NaN.
% Run: build/desktop-fast/tests/smoke/Release/numkit_smoke.exe \
%      toolboxes/builtin/tests/smoke/power_complex_smoke.m

s = @(z) sprintf('%.5g%+.5gi', real(z), imag(z));

fprintf('--- negative base ^ non-integer exp -> complex ---\n');
fprintf('(-8)^(1/3)    = %s (expect 1+1.7321i)\n', s((-8)^(1/3)));
fprintf('power(-8,1/3) = %s (expect 1+1.7321i)\n', s(power(-8,1/3)));
fprintf('(-2)^0.5      = %s (expect 0+1.4142i)\n', s((-2)^0.5));
t = -8; fprintf('t=-8; t^(1/3)  = %s (expect 1+1.7321i)\n', s(t^(1/3)));

fprintf('\n--- integer exp / positive base stay real ---\n');
fprintf('(-8)^2=%g (-8)^3=%g 8^(1/3)=%g 2^10=%g (expect 64 -512 2 1024)\n', ...
        (-8)^2, (-8)^3, 8^(1/3), 2^10);

fprintf('\n--- arrays promote per-pair ---\n');
fprintf('[-8 8].^[2 0.5] isreal=%d: ', isreal([-8 8].^[2 0.5]));
w = [-8 8].^[2 0.5]; fprintf('%s , %s (expect 64, 2.828, REAL)\n', s(w(1)), s(w(2)));
x = [-8 8].^[0.5 2];
fprintf('[-8 8].^[0.5 2]: %s , %s (expect 0+2.828i, 64)\n', s(x(1)), s(x(2)));
p = power([-8 8 -27], 1/3);
fprintf('power([-8 8 -27],1/3): %s , %s , %s (expect 1+1.73i, 2, 1.5+2.60i)\n', ...
        s(p(1)), s(p(2)), s(p(3)));
