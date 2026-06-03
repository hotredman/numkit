clear
import compat.*
% unique 'last' — ia selects the LAST occurrence of each value (sorted order).
% The default 'first' is unchanged.
[c, ia] = unique([3 1 2 1 3], 'last');
fprintf('vec last:    C=[%g %g %g]  ia=[%g %g %g]  (expect C=1 2 3, ia=4 3 5)\n', ...
        c(1), c(2), c(3), ia(1), ia(2), ia(3));

[c2, ia2] = unique([3 1 2 1 3]);            % default 'first'
fprintf('vec first:   ia=[%g %g %g]  (expect 2 3 1)\n', ia2(1), ia2(2), ia2(3));

[cc, iac] = unique([1+1i 2 1+1i 3], 'last');
fprintf('complex last: ia=[%g %g %g]  (expect 3 2 4)\n', iac(1), iac(2), iac(3));

[cr, iar] = unique([1 2; 3 4; 1 2; 5 6], 'rows', 'last');
fprintf('rows last:   ia=[%g %g %g]  (expect 3 2 4)\n', iar(1), iar(2), iar(3));
