clear

import compat.*

% nchoosek — scalar binomial coefficient AND vector combinations form.
% Vector form added 2026-05-29 (DEEP-PROBE). vs MATLAB R2025b.

fprintf('=== scalar binomial nchoosek(N,K) ===\n');
fprintf('nchoosek(5,2)  = %g (expect 10)\n',  nchoosek(5,2));
fprintf('nchoosek(10,3) = %g (expect 120)\n', nchoosek(10,3));
fprintf('nchoosek(7,0)  = %g (expect 1)\n',   nchoosek(7,0));
fprintf('nchoosek(7,7)  = %g (expect 1)\n',   nchoosek(7,7));

fprintf('\n=== vector form nchoosek(V,K) -> all K-combinations, one per row ===\n');
C = nchoosek([1 2 3 4], 2);
fprintf('nchoosek([1 2 3 4],2) is %dx%d (expect 6x2):\n', size(C,1), size(C,2));
disp(C);   % expect [1 2;1 3;1 4;2 3;2 4;3 4]

D = nchoosek([10 20 30], 2);
fprintf('nchoosek([10 20 30],2) (values from V, expect [10 20;10 30;20 30]):\n');
disp(D);

E = nchoosek([5 6 7], 3);
fprintf('k==numel -> single row, %dx%d (expect 1x3): ', size(E,1), size(E,2));
disp(E);   % expect [5 6 7]

F = nchoosek([1 2 3], 0);
fprintf('k==0 -> %dx%d (expect 1x0)\n', size(F,1), size(F,2));

G = nchoosek([9 8 7 6], 1);
fprintf('k==1 -> %dx%d (expect 4x1), column = [9;8;7;6]\n', size(G,1), size(G,2));
