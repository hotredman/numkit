clear
import compat.*

% fzero / fminbnd / fminsearch [x, fval, exitflag] multi-output.

[x, fval, ef] = fzero(@(x) x.^2 - 4, [0 10]);
fprintf('fzero    : x=%.10g fval=%.3e exitflag=%d\n', x, fval, ef);   % 2, 0, 1

[xb, fb, eb] = fminbnd(@(x) (x-3).^2 + 1, 0, 10);
fprintf('fminbnd  : x=%.10g fval=%.10g exitflag=%d\n', xb, fb, eb);   % 3, 1, 1

[xs, fs, es] = fminsearch(@(v) (v(1)-1)^2 + (v(2)-2)^2, [0 0]);
fprintf('fminsearch: x=%s fval=%.3e exitflag=%d\n', mat2str(xs,6), fs, es); % ~[1 2], ~0, 1
