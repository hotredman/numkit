clear
import compat.*

% median of a COMPLEX array — order by abs, ties by angle (MATLAB).
% Fixed 2026-06-05: previously "complex inputs are not supported".

fprintf('median([1+1i 2+2i 3+3i])        = %g%+gi   (expect 2+2i)\n', ...
        real(median([1+1i 2+2i 3+3i])), imag(median([1+1i 2+2i 3+3i])));

m2 = median([1+1i 2+2i 3+3i 10+10i]);
fprintf('median([..10+10i]) (even)        = %g%+gi   (expect 2.5+2.5i)\n', real(m2), imag(m2));

cm = median([1+1i 2; 3 4i]);
fprintf('median(M) col-wise               = %g%+gi, %g%+gi   (expect 2+0.5i, 1+2i)\n', ...
        real(cm(1)),imag(cm(1)), real(cm(2)),imag(cm(2)));

md = median([1+1i 5 2-3i], 2);
fprintf('median(row, 2)                   = %g%+gi   (expect 2-3i)\n', real(md), imag(md));

mt = median([1 1i -1]);
fprintf('median([1 1i -1]) (abs tie)      = %g%+gi   (expect 0+1i)\n', real(mt), imag(mt));

ma = median([1+1i 2; 3 4i], 'all');
fprintf('median(M, ''all'')                 = %g%+gi   (expect 2.5+0i)\n', real(ma), imag(ma));
