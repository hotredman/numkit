clear

import compat.*

% flip / fliplr / flipud / rot90 on CELL and STRING arrays (2026-05-31).
% MATLAB's flip family is type-agnostic — it just permutes elements, so
% cell and string arrays reorder exactly like numeric ones. numkit
% previously errored ("ND fallback does not support type cell/string" /
% "Not a double array"). vs MATLAB R2025b.

fprintf('=== flip / fliplr / flipud on a cell vector ===\n');
c = flip({10, 20, 30});
fprintf('flip({10,20,30})   = {%d %d %d}  (expect {30 20 10})\n', c{1}, c{2}, c{3});
c = fliplr({10, 20, 30});
fprintf('fliplr({10,20,30}) = {%d %d %d}  (expect {30 20 10})\n', c{1}, c{2}, c{3});
c = flipud({10; 20; 30});
fprintf('flipud({10;20;30}) = {%d %d %d}  (expect {30 20 10})\n', c{1}, c{2}, c{3});

fprintf('\n=== flip on a 2x2 cell along each dim ===\n');
c = flip({1 2; 3 4}, 2);
fprintf('flip(C,2) row1 = {%d %d}  (expect {2 1})\n', c{1,1}, c{1,2});
c = flip({1 2; 3 4}, 1);
fprintf('flip(C,1) col1 = {%d %d}  (expect {3 1})\n', c{1,1}, c{2,1});

fprintf('\n=== rot90 on a 2x2 cell (k = 1, 2, 3) ===\n');
c = rot90({1 2; 3 4});
fprintf('rot90(C)   = {%d %d; %d %d}  (expect {2 4; 1 3})\n', c{1,1}, c{1,2}, c{2,1}, c{2,2});
c = rot90({1 2; 3 4}, 2);
fprintf('rot90(C,2) = {%d %d; %d %d}  (expect {4 3; 2 1})\n', c{1,1}, c{1,2}, c{2,1}, c{2,2});
c = rot90({1 2; 3 4}, 3);
fprintf('rot90(C,3) = {%d %d; %d %d}  (expect {3 1; 4 2})\n', c{1,1}, c{1,2}, c{2,1}, c{2,2});

fprintf('\n=== rot90 reshapes a 1x3 cell to 3x1 ===\n');
c = rot90({1, 2, 3});
fprintf('size(rot90({1,2,3})) = [%d %d]  (expect [3 1])\n', size(c,1), size(c,2));

fprintf('\n=== flip / rot90 on string arrays (char codes) ===\n');
s = flip(["x" "y" "z"]);
fprintf('flip(["x" "y" "z"]) codes = [%d %d %d]  (expect [122 121 120])\n', ...
        double(char(s(1))), double(char(s(2))), double(char(s(3))));
s = rot90(["a" "b"; "c" "d"]);
fprintf('rot90 string (1,1)=%d (2,2)=%d  (expect 98 99)\n', ...
        double(char(s(1,1))), double(char(s(2,2))));

fprintf('\n=== cellstr flip ===\n');
c = flip({'aa', 'bb', 'cc'});
fprintf('flip({aa,bb,cc}) = {%s %s %s}  (expect {cc bb aa})\n', c{1}, c{2}, c{3});
