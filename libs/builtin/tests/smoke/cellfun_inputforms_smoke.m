clear
import compat.*

% cellfun multi-cell + legacy string-name forms. Fixed 2026-06-05
% (bugs/builtin/cellfun-inputforms.md). Reference: MATLAB R2025b.

% --- multiple cell arrays: fn(C1{i}, C2{i}, ...) ---
r2 = cellfun(@(a,b) a+b, {1,2,3}, {10,20,30});
fprintf('cellfun(@(a,b)a+b, {1 2 3},{10 20 30}) = %g %g %g  (expect 11 22 33)\n', r2(1),r2(2),r2(3));

r3 = cellfun(@(a,b,c) a+b+c, {1,2}, {10,20}, {100,200});
fprintf('3 cells = %g %g  (expect 111 222)\n', r3(1),r3(2));

% UniformOutput false with multi-cell
rc = cellfun(@(a,b) a*b, {2,3}, {5,7}, 'UniformOutput', false);
fprintf('multi-cell UniformOutput=false: {%g, %g}  (expect 10 21)\n', rc{1}, rc{2});

% --- legacy string-function names ---
ie = cellfun('isempty', {[],[1],[]});
fprintf('isempty   = %g %g %g  (expect 1 0 1)\n', ie(1),ie(2),ie(3));
ln = cellfun('length', {[1 2],[1 2 3]});
fprintf('length    = %g %g     (expect 2 3)\n', ln(1),ln(2));
nd = cellfun('ndims', {1, ones(2,2,2)});
fprintf('ndims     = %g %g     (expect 2 3)\n', nd(1),nd(2));
ps = cellfun('prodofsize', {[1 2 3], ones(2,3)});
fprintf('prodofsize= %g %g     (expect 3 6)\n', ps(1),ps(2));
ir = cellfun('isreal', {1, 1+2i});
fprintf('isreal    = %g %g     (expect 1 0)\n', ir(1),ir(2));
il = cellfun('islogical', {true, 1});
fprintf('islogical = %g %g     (expect 1 0)\n', il(1),il(2));
sz = cellfun('size', {[1 2 3], ones(2,4)}, 2);
fprintf('size dim2 = %g %g     (expect 3 4)\n', sz(1),sz(2));
ic = cellfun('isclass', {1, int8(2), 'str'}, 'double');
fprintf('isclass   = %g %g %g  (expect 1 0 0)\n', ic(1),ic(2),ic(3));
