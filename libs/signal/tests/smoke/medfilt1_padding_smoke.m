clear

import compat.*

% medfilt1 windowing + padding (2026-05-30): MATLAB's window for sample i
% is [i-floor(k/2) .. i+ceil(k/2)-1] (even k leans LEFT) and the DEFAULT
% zero-pads the ends; 'truncate' clips the window instead. numkit
% previously (a) truncated by default and (b) used a right-leaning even
% window. vs MATLAB R2025b.

x = [2 80 6 3 10 8];

fprintf('=== odd k=3, DEFAULT zeropad ===\n');
fprintf('medfilt1(x,3)            = %s  (expect [2 6 6 6 8 8])\n', mat2str(medfilt1(x,3)));
fprintf('medfilt1(x,3,''truncate'') = %s  (expect [41 6 6 6 8 9])\n', mat2str(medfilt1(x,3,'truncate')));

fprintf('\n=== even k=4 (window leans left) ===\n');
fprintf('medfilt1(x,4)            = %s  (expect [1 4 4.5 8 7 5.5])\n', mat2str(medfilt1(x,4)));
fprintf('medfilt1(x,4,''truncate'') = %s  (expect [41 6 4.5 8 7 8])\n', mat2str(medfilt1(x,4,'truncate')));

fprintf('\n=== even k=2, odd k=5 ===\n');
fprintf('medfilt1(x,2) = %s  (expect [1 41 43 4.5 6.5 9])\n', mat2str(medfilt1(x,2)));
fprintf('medfilt1(x,5) = %s  (expect [2 3 6 8 6 3])\n', mat2str(medfilt1(x,5)));

fprintf('\n=== matrix: each column filtered independently ===\n');
disp(medfilt1([1 2;3 4;5 6;7 8], 3));
fprintf('expect [1 2; 3 4; 5 6; 5 6]\n');
