clear

import compat.*

% bugs/builtin/unique-typeclass.md — unique on char / logical / integer.
% MATLAB PRESERVES the input class on the unique VALUES; ia/ic stay double.

[uc, ia, ic] = unique('cbabc');
fprintf('unique(''cbabc'') = %s   expect abc, ischar=%d (expect 1)\n', uc, ischar(uc));
fprintf('  ia = [%g %g %g]   expect [3 2 1] (first-occurrence)\n', ia(1), ia(2), ia(3));
fprintf('  ic = [%g %g %g %g %g]   expect [3 2 1 2 3]\n', ic(1), ic(2), ic(3), ic(4), ic(5));

us = unique('cbabc', 'stable');
fprintf('unique(''cbabc'',''stable'') = %s   expect cba\n', us);

ul = unique(logical([1 0 1 1]));
fprintf('unique(logical([1 0 1 1])) = [%g %g]   expect [0 1], islogical=%d (expect 1)\n', ...
        ul(1), ul(2), islogical(ul));

uj = unique(int8([3 1 3 2]));
fprintf('unique(int8([3 1 3 2])) = [%g %g %g]   expect [1 2 3], isa int8=%d (expect 1)\n', ...
        double(uj(1)), double(uj(2)), double(uj(3)), isa(uj,'int8'));

uk = unique(uint16([30;10;30;20]));
fprintf('unique(uint16 col) size = [%g %g]   expect [3 1] (column preserved), isa uint16=%d (expect 1)\n', ...
        size(uk,1), size(uk,2), isa(uk,'uint16'));

R = unique(logical([1 0; 1 0; 0 1]), 'rows');
fprintf('unique(logical rows) = [%g %g; %g %g]   expect [0 1; 1 0], islogical=%d (expect 1)\n', ...
        double(R(1,1)), double(R(1,2)), double(R(2,1)), double(R(2,2)), islogical(R));
