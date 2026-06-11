clear
import compat.*
% rc2poly — reflection coefficients -> AR poly (step-up), + efinal output.
a = rc2poly([-0.5 0.4 0.2]);
fprintf('a = [%.3f %.3f %.3f %.3f] (expect 1 -0.62 0.26 0.2)\n', a(1), a(2), a(3), a(4));

% Two-output form: [a, efinal] = rc2poly(k, r0), efinal = r0*prod(1-k.^2).
[a2, e2] = rc2poly([0.5 0.3], 4);
fprintf('a2 = [%.2f %.2f %.2f]  efinal = %.4f (expect 1 0.65 0.3  2.73)\n', a2(1), a2(2), a2(3), e2);
