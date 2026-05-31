clear
import compat.*
% str2double on a CELL array / non-scalar string array -> DOUBLE matrix of
% the same shape, NaN where an element doesn't parse. DEEP-PROBE 2026-05-31:
% numkit used to throw "Not a char array". Values pinned vs MATLAB R2025b.

a = str2double({'1.5','2.5','x'});
fprintf('a = [%g %g %g]   (expect 1.5 2.5 NaN)\n', a(1), a(2), a(3));

b = str2double({'1';'2';'3'});
fprintf('b is %dx%d (expect 3x1), b(2)=%g (expect 2)\n', size(b,1), size(b,2), b(2));

c = str2double({'1','2';'3','4'});
fprintf('c is %dx%d (expect 2x2), c(2,1)=%g c(1,2)=%g (expect 3 2)\n', ...
        size(c,1), size(c,2), c(2,1), c(1,2));

d = str2double({' 42 ','1,234','Inf','-Inf','NaN',''});
fprintf('d = [%g %g %g %g %g %g]  (expect 42 1234 Inf -Inf NaN NaN)\n', ...
        d(1), d(2), d(3), d(4), d(5), d(6));

e = str2double(["10" "20" "abc"]);
fprintf('e = [%g %g %g]   (expect 10 20 NaN)\n', e(1), e(2), e(3));

% Scalar form is unchanged.
fprintf('scalar str2double(''3.14'') = %g   (expect 3.14)\n', str2double('3.14'));
