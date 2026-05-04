clear

import compat.*

% bestblk — best block size for block-processing.

fprintf('--- Octave-source reference vectors ---\n');
fprintf('bestblk([10 12], 2)   = %s (expect [2 2])\n',     mat2str(bestblk([10 12], 2)));
fprintf('bestblk([10 12], 3)   = %s (expect [2 3])\n',     mat2str(bestblk([10 12], 3)));
fprintf('bestblk([300 100], 150)= %s (expect [150 100])\n', mat2str(bestblk([300 100], 150)));
fprintf('bestblk([256 128], 17)= %s (expect [16 16])\n',   mat2str(bestblk([256 128], 17)));
fprintf('bestblk([17 17], 3)   = %s (expect [3 3])\n',     mat2str(bestblk([17 17], 3)));

fprintf('\n--- N-D ---\n');
fprintf('bestblk([10 12 10], 3)    = %s (expect [2 3 2])\n', mat2str(bestblk([10 12 10], 3)));
fprintf('bestblk([9 12 9], 3)      = %s (expect [3 3 3])\n', mat2str(bestblk([9 12 9], 3)));
fprintf('bestblk([10 12 10 11], 5) = %s (expect [5 4 5 4])\n', mat2str(bestblk([10 12 10 11], 5)));

fprintf('\n--- default k=100 ---\n');
fprintf('bestblk([230 470]) = %s\n', mat2str(bestblk([230 470])));
