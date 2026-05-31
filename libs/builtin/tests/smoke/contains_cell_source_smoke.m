clear
import compat.*
% contains / startsWith / endsWith on a CELL array (or non-scalar string
% array) SOURCE -> LOGICAL array of the same shape (any-match over the
% pattern set). DEEP-PROBE 2026-05-31: numkit threw "Not a char array" on a
% cell source / returned a scalar on a string array. Pinned vs MATLAB R2025b.

fprintf('contains cell  : '); disp(double(contains({'hello','world','help'},'el')));   % 1 0 1
fprintf('startsWith cell: '); disp(double(startsWith({'hello','world','help'},'he'))); % 1 0 1
fprintf('endsWith strarr: '); disp(double(endsWith(["cat.txt","dog.csv","fish.txt"],'.txt'))); % 1 0 1

m = contains({'ab','cd';'be','xy'},'b');
fprintf('2x2 shape %dx%d, [m11 m21 m12 m22]=', size(m,1), size(m,2));
disp(double([m(1,1) m(2,1) m(1,2) m(2,2)]));   % 1 1 0 0

fprintf('cell + IgnoreCase: '); disp(double(startsWith({'Hello','world'},'he','IgnoreCase',true))); % 1 0
fprintf('multi-pattern    : '); disp(double(contains({'foo','bar','baz'},{'oo','az'})));            % 1 0 1

fprintf('scalar source still scalar: %d (islogical=%d)\n', ...
        contains('hello','ell'), islogical(contains('hello','ell')));
