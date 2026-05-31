clear
import compat.*
% pammod / pamdemod — M-PAM. DEFAULT symbol order is 'bin' (NOT gray).
yb = real(pammod([0 1 2 3], 4));
fprintf('M=4 bin : %g %g %g %g (expect -3 -1 1 3)\n', yb(1), yb(2), yb(3), yb(4));

yg = real(pammod([0 1 2 3], 4, 0, 'gray'));
fprintf('M=4 gray: %g %g %g %g (expect -3 -1 3 1)\n', yg(1), yg(2), yg(3), yg(4));

x = pamdemod(yb, 4);
fprintf('demod   : %g %g %g %g (expect 0 1 2 3)\n', x(1), x(2), x(3), x(4));
