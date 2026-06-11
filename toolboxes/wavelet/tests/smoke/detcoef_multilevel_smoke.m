clear
import compat.*

% detcoef now supports MATLAB's multi-level forms. Previously only a SCALAR
% level (and the explicit [levels],'cells' form) worked; a bare vector of
% levels threw "level must be scalar".
%   detcoef(C, L, [n1 n2 ...])         -> CELL array of per-level details
%   detcoef(C, L, 'cells')             -> CELL of ALL levels 1..nMax
%   [d1, d2, ...] = detcoef(C, L, [..]) -> one detail per output

[c, l] = wavedec(1:16, 3, 'db1');

fprintf('--- vector of levels -> cell ---\n');
cv = detcoef(c, l, [1 2 3]);
fprintf('iscell=%d numel=%d  lengths: %d %d %d   (expect 1, 3, 8 4 2)\n', ...
        iscell(cv), numel(cv), numel(cv{1}), numel(cv{2}), numel(cv{3}));
fprintf('cv{1}(1)=%.6f  cv{3}(1)=%.6f   (expect -0.707107, -5.656854)\n', ...
        cv{1}(1), cv{3}(1));

fprintf('--- ''cells'' string -> all levels ---\n');
ca = detcoef(c, l, 'cells');
fprintf('iscell=%d numel=%d   (expect 1, 3)\n', iscell(ca), numel(ca));

fprintf('--- multi-output deal ---\n');
[d1, d2, d3] = detcoef(c, l, [1 2 3]);
fprintf('numel d1=%d d2=%d d3=%d   d1(1)=%.6f d3(1)=%.6f\n', ...
        numel(d1), numel(d2), numel(d3), d1(1), d3(1));

fprintf('--- scalar form unchanged ---\n');
ds = detcoef(c, l, 2);
fprintf('iscell=%d numel=%d ds(1)=%.6f   (expect 0, 4, -2)\n', ...
        iscell(ds), numel(ds), ds(1));
