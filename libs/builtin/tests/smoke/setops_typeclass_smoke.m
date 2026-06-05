clear

import compat.*

% bugs/builtin/setops-typeclass.md — ismember/intersect/setdiff/union on
% char / logical / integer. MATLAB accepts all: intersect/setdiff/union
% PRESERVE the input class on the values; ismember -> logical tf + double loc.

fprintf('ismember(''b'',''abcd'') = %g   expect 1\n', ismember('b','abcd'));
[tf, loc] = ismember('xbq','abcd');
fprintf('[tf,loc]=ismember(''xbq'',''abcd''): tf=[%g %g %g] loc=[%g %g %g]   expect tf=[0 1 0] loc=[0 2 0]\n', ...
        tf(1), tf(2), tf(3), loc(1), loc(2), loc(3));
fprintf('  islogical(tf)=%d (expect 1), islogical(loc)=%d (expect 0)\n', islogical(tf), islogical(loc));

[c, ia, ib] = intersect('cabc','bdc');
fprintf('intersect(''cabc'',''bdc'') = %s   expect bc, ischar=%d (expect 1)\n', c, ischar(c));
fprintf('  ia=[%g %g] ib=[%g %g]   expect ia=[3 1] ib=[1 3]\n', ia(1), ia(2), ib(1), ib(2));

fprintf('setdiff(''abce'',''bd'') = %s   expect ace\n', setdiff('abce','bd'));
fprintf('union(''ab'',''bc'') = %s   expect abc\n', union('ab','bc'));
fprintf('union(''bca'',''db'',''stable'') = %s   expect bcad\n', union('bca','db','stable'));

cl = intersect(logical([1 0 1]), logical([0 0 1]));
fprintf('intersect(logical,logical) = [%g %g]   expect [0 1], islogical=%d (expect 1)\n', ...
        double(cl(1)), double(cl(2)), islogical(cl));

cj = intersect(int8([3 1 2]), int8([2 4 1]));
fprintf('intersect(int8,int8) = [%g %g]   expect [1 2], isa int8=%d (expect 1)\n', ...
        double(cj(1)), double(cj(2)), isa(cj,'int8'));
