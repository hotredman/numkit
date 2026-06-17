clear

import compat.*

% unique output orientation. Bug fixed 2026-05-30: ia/ic came back as row
% vectors and u was always a row even for a column input. MATLAB: ia/ic are
% ALWAYS columns; u matches the input orientation. vs MATLAB R2025b.

fprintf('=== row input ===\n');
[u, ia, ic] = unique([3 1 2 1 3]);
fprintf('u   %dx%d (expect 1x3, a row)\n', size(u,1), size(u,2));
fprintf('ia  %dx%d = %s (expect 3x1 [2;3;1])\n', size(ia,1), size(ia,2), mat2str(ia));
fprintf('ic  %dx%d = %s (expect 5x1 [3;1;2;1;3])\n', size(ic,1), size(ic,2), mat2str(ic));

fprintf('\n=== column input ===\n');
[uc, iac, icc] = unique([3;1;2;1;3]);
fprintf('u   %dx%d (expect 3x1, a column)\n', size(uc,1), size(uc,2));
fprintf('ia  %dx%d (expect 3x1)\n', size(iac,1), size(iac,2));

fprintf('\n=== single output keeps input orientation ===\n');
fprintf('unique(col) = %s (expect [1;2;3])\n', mat2str(unique([3;1;2;1;3])));
fprintf('unique(row) = %s (expect [1 2 3])\n', mat2str(unique([3 1 2 1 3])));

fprintf('\n=== ''stable'' indices are columns too ===\n');
[us, ias, ics] = unique([3 1 4 1 5], 'stable');
fprintf('ia %dx%d ic %dx%d (expect 4x1 5x1)\n', size(ias,1), size(ias,2), size(ics,1), size(ics,2));
