clear
import compat.*
% cross / kron on COMPLEX inputs: ordinary complex arithmetic, NO conjugation
% (unlike dot). numkit previously threw / rejected complex. MATLAB R2025b.

fprintf('cross([1+1i 0 0],[0 1 0]) = %s (expect [0 0 1+1i])\n', mat2str(cross([1+1i 0 0],[0 1 0])));
cc = cross([1i; 2; 3], [4; 5i; 6]);
fprintf('cross col = %s (expect [12-15i 12-6i -13])\n', mat2str(cc.'));

fprintf('kron([1i 2],[1 1]) = %s (expect [1i 1i 2 2])\n', mat2str(kron([1i 2],[1 1])));
disp('kron([1+1i;2],[1 1i]) (expect [1+1i -1+1i; 2 2i]):');
disp(kron([1+1i; 2], [1 1i]));

% real paths unchanged.
fprintf('cross([1 2 3],[4 5 6]) = %s (expect [-3 6 -3])\n', mat2str(cross([1 2 3],[4 5 6])));
fprintf('kron([1 2],[3 4]) = %s (expect [3 4 6 8])\n', mat2str(kron([1 2],[3 4])));
