clear

import compat.*

% pad on a CELL str + the default-width form — DEEP-PROBE 2026-05-31. pad
% required an explicit width n and threw "Not a char array" on a cell. MATLAB:
% pad(s) defaults the width to the LONGEST element (own length for a scalar);
% a cell str is padded element-wise -> cell of char vectors, same shape. The
% 'both' side splits extra padding floor-left / ceil-right. Reference: MATLAB
% R2025b.

fprintf('=== default width (longest element) ===\n');
d = pad({'a','bbb'});
fprintf('pad({a,bbb}) -> [%s][%s]  lens %d %d  (expect "a  ","bbb" / 3,3)\n', ...
        d{1}, d{2}, strlength(d{1}), strlength(d{2}));

fprintf('\n=== explicit width / side / char ===\n');
cn = pad({'a','bb'}, 4);
fprintf('n=4    -> [%s][%s]  (expect "a   ","bb  ")\n', cn{1}, cn{2});
lf = pad({'a','bb'}, 4, 'left');
fprintf('left   -> [%s][%s]  (expect "   a","  bb")\n', lf{1}, lf{2});
bt = pad({'a','bb'}, 4, 'both', '*');
fprintf('both * -> [%s][%s]  (expect *a**,*bb*)\n', bt{1}, bt{2});

fprintf('\n=== shape preserved + scalar paths unchanged ===\n');
cc = pad({'a';'bbb'});
fprintf('column cell -> size %dx%d  (expect 2x1)\n', size(cc,1), size(cc,2));
fprintf('scalar pad(a) = [%s] (len %d); pad(hi,5,right,-) = [%s]\n', pad('a'), strlength(pad('a')), pad('hi',5,'right','-'));
