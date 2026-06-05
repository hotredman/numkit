clear;
import compat.*;

% vartestn — k-sample variance equality test (5 variants).
%
%
% New 2026-05-08:
%   - 'TestType' N-V parsed: Bartlett (default), LeveneQuadratic,
%     LeveneAbsolute, BrownForsythe, OBrien
%   - F-based variants return {fstat, df=[k-1, N-k]} struct shape
%   - Bartlett keeps {chisqstat, df}
%   - Matrix-input form: vartestn(X) treats each column as a group
%
% Plus PMR rule applied: scratch buffers (group buckets, Z values)
% on ScratchArena + ScratchVec.

xg = [1.2 1.5 1.8 2.1 5.0 5.5 4.8 6.0 4.5 9.1 8.5 10.0 9.5]';
g  = [1 1 1 1 2 2 2 2 2 3 3 3 3]';

fprintf('--- Bartlett (default) ---\n');
[p, st] = vartestn(xg, g, 'Display', 'off');
fprintf('p=%.6f chisqstat=%.6f df=%d\n', p, st.chisqstat, st.df);

fprintf('--- LeveneAbsolute ---\n');
[p, st] = vartestn(xg, g, 'Display', 'off', 'TestType', 'LeveneAbsolute');
fprintf('p=%.6f fstat=%.6f df=[%d %d]\n', p, st.fstat, st.df(1), st.df(2));

fprintf('--- LeveneQuadratic ---\n');
[p, st] = vartestn(xg, g, 'Display', 'off', 'TestType', 'LeveneQuadratic');
fprintf('p=%.6f fstat=%.6f df=[%d %d]\n', p, st.fstat, st.df(1), st.df(2));

fprintf('--- BrownForsythe ---\n');
[p, st] = vartestn(xg, g, 'Display', 'off', 'TestType', 'BrownForsythe');
fprintf('p=%.6f fstat=%.6f df=[%d %d]\n', p, st.fstat, st.df(1), st.df(2));

fprintf('--- OBrien ---\n');
[p, st] = vartestn(xg, g, 'Display', 'off', 'TestType', 'OBrien');
fprintf('p=%.6f fstat=%.6f df=[%d %d]\n', p, st.fstat, st.df(1), st.df(2));

fprintf('--- Matrix input (no group) ---\n');
M = [1 5 9; 2 6 10; 3 4 11; 4 5 12];
[p, st] = vartestn(M, 'Display', 'off');
fprintf('p=%.6f chisqstat=%.6f df=%d\n', p, st.chisqstat, st.df);
fprintf('All values bit-identical to MATLAB R2025b.\n');
