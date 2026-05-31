clear

import compat.*

% circshift(X, K, dim) shifts by K ONLY along dimension `dim`. The 3-arg
% dim form was fixed 2026-05-30 (DEEP-PROBE — it previously ignored dim and
% always shifted dim 1). vs MATLAB R2025b.

fprintf('=== explicit dim ===\n');
fprintf('circshift([1 2 3;4 5 6],1,2) = %s (expect [3 1 2;6 4 5], columns)\n', ...
        mat2str(circshift([1 2 3;4 5 6],1,2)));
fprintf('circshift([1 2 3;4 5 6],1,1) = %s (expect [4 5 6;1 2 3], rows)\n', ...
        mat2str(circshift([1 2 3;4 5 6],1,1)));
fprintf('circshift([1 2 3;4 5 6],-1,2) = %s (expect [2 3 1;5 6 4])\n', ...
        mat2str(circshift([1 2 3;4 5 6],-1,2)));
fprintf('circshift([10 20 30 40],2,2) = %s (expect [30 40 10 20], row dim 2)\n', ...
        mat2str(circshift([10 20 30 40],2,2)));

fprintf('\n=== regress (2-arg + vector form unchanged) ===\n');
fprintf('circshift([1 2 3;4 5 6],1)   = %s (expect [4 5 6;1 2 3])\n', mat2str(circshift([1 2 3;4 5 6],1)));
fprintf('circshift([1 2 3;4 5 6],[1 1]) = %s (expect [6 4 5;3 1 2])\n', mat2str(circshift([1 2 3;4 5 6],[1 1])));
fprintf('circshift([1 2 3 4 5],2)     = %s (expect [4 5 1 2 3])\n', mat2str(circshift([1 2 3 4 5],2)));
