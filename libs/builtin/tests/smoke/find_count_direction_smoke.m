clear
import compat.*

% find(X, K)            -> first K nonzero indices
% find(X, K, 'last')    -> last K nonzero indices (ascending order)
% Bug fixed 2026-06-05: K + direction were ignored (returned all nonzeros).

v = [0 1 0 1 1];
fprintf('find(v, 2)        = '); disp(find(v, 2));         % expect [2 4]
fprintf('find(v, 1, last)  = '); disp(find(v, 1, 'last')); % expect 5
fprintf('find(v, 2, last)  = '); disp(find(v, 2, 'last')); % expect [4 5]
fprintf('find(v, 2, first) = '); disp(find(v, 2, 'first'));% expect [2 4]
fprintf('find(v)           = '); disp(find(v));            % expect [2 4 5]
fprintf('find(v, 10)       = '); disp(find(v, 10));        % expect [2 4 5] (K>count)

A = [0 3; 4 0];
[r, c] = find(A, 1);
fprintf('[r,c]=find(A,1)        -> r=%d c=%d  (expect r=2 c=1)\n', r, c);
[r2, c2] = find(A, 1, 'last');
fprintf('[r,c]=find(A,1,last)   -> r=%d c=%d  (expect r=1 c=2)\n', r2, c2);
[r3, c3, val] = find(A, 1, 'last');
fprintf('[r,c,v]=find(A,1,last) -> v=%d        (expect v=3)\n', val);
