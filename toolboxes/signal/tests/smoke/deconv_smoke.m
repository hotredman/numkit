clear
import compat.*
% deconv: polynomial long division [Q, R] = deconv(B, A), B = conv(A,Q) + R.
[q, r] = deconv([1 2 3 4], [1 1]);
fprintf('deconv([1 2 3 4],[1 1]): Q='); fprintf('%g ', q);
fprintf(' R='); fprintf('%g ', r); fprintf(' (expect Q=1 1 2, R=0 0 0 2)\n');

% Divisor longer than dividend -> Q = 0 (scalar), R = numerator unchanged.
[q2, r2] = deconv([1 2 3], [1 2 3 4]);
fprintf('deconv([1 2 3],[1 2 3 4]): Q='); fprintf('%g ', q2);
fprintf(' R='); fprintf('%g ', r2); fprintf(' (expect Q=0, R=1 2 3)\n');

[q3, r3] = deconv(5, [1 2]);
fprintf('deconv(5,[1 2]): Q=%g R=%g (expect Q=0, R=5)\n', q3, r3);
