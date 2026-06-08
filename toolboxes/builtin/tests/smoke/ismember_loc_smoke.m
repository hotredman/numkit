clear
import compat.*
% ismember 2nd output loc = lowest 1-based index in B (0 if absent).
[tf, loc] = ismember([2 5 8 1], [5 2 9]);
fprintf('tf =[%g %g %g %g] (expect 1 1 0 0)\n', tf(1),tf(2),tf(3),tf(4));
fprintf('loc=[%g %g %g %g] (expect 2 1 0 0)\n', loc(1),loc(2),loc(3),loc(4));

% Tie: duplicate values in B -> lowest index.
[~, l2] = ismember([3 1 2], [2 1 3 1]);
fprintf('tie loc=[%g %g %g] (expect 3 2 1)\n', l2(1),l2(2),l2(3));

% Complex ismember: exact-equality membership, Locb lowest index. MATLAB R2025b.
[ctf, cloc] = ismember([1 5i 3+4i 2], [3+4i 5i 1]);
fprintf('\ncomplex tf =[%g %g %g %g] (expect 1 1 1 0)\n', ctf(1),ctf(2),ctf(3),ctf(4));
fprintf('complex loc=[%g %g %g %g] (expect 3 2 1 0)\n', cloc(1),cloc(2),cloc(3),cloc(4));
fprintf('real-vs-complex 2 in [2+0i 5i] = %g (expect 1)\n', double(ismember(2, [2+0i 5i])));
fprintf('NaN component never matches = %g (expect 0)\n', double(ismember(complex(nan,1), [complex(nan,1) 2])));
