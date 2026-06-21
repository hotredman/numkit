clear
import compat.*

% Steiglitz-McBride IIR identification -- bugs/signal/stmcb.
% Recover B(z)/A(z) (nb zeros, na poles) from an approximate impulse response h.
% Init A via prony, then 5 SM iterations (prefilter by 1/A, re-solve LS).

[b1, a1] = stmcb([1 0.5 0.25 0.125 0.0625], 1, 1);
fprintf('1st order: b=[%.4f %.4f]  a=[%.4f %.4f]\n', b1(1), b1(2), a1(1), a1(2));
fprintf('  expect:  b~[1 0]         a=[1 -0.5]\n');

h = filter([1 0.3], [1 -0.6 0.2], [1 zeros(1,29)]);
[b2, a2] = stmcb(h, 1, 2);
fprintf('2nd order: b=[%.4f %.4f]  a=[%.4f %.4f %.4f]\n', b2(1), b2(2), a2(1), a2(2), a2(3));
fprintf('  expect:  b=[1.0000 0.3000]  a=[1.0000 -0.6000 0.2000]\n');
