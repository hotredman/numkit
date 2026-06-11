clear
import compat.*

fprintf('=== matrix-structure predicates ===\n');

% --- issymmetric ---
fprintf('\n[issymmetric]\n');
fprintf('  [1 2; 2 1]            -> %d (expect 1)\n', issymmetric([1 2; 2 1]));
fprintf('  [1 2; 3 1]            -> %d (expect 0)\n', issymmetric([1 2; 3 1]));
fprintf('  [1+1i 2; 2 1-1i]      -> %d (expect 1, transpose-no-conj)\n', issymmetric([1+1i 2; 2 1-1i]));
fprintf('  [0 -2; 2 0] skew      -> %d (expect 1)\n', issymmetric([0 -2; 2 0], 'skew'));
fprintf('  [1 2; 2 1] skew       -> %d (expect 0)\n', issymmetric([1 2; 2 1], 'skew'));

% --- ishermitian ---
fprintf('\n[ishermitian]\n');
fprintf('  [1 1i; -1i 1]         -> %d (expect 1)\n', ishermitian([1 1i; -1i 1]));
fprintf('  [1 1i; 1i 1]          -> %d (expect 0)\n', ishermitian([1 1i; 1i 1]));
fprintf('  [0 1i; 1i 0] skew     -> %d (expect 1)\n', ishermitian([0 1i; 1i 0], 'skew'));

% --- isbanded ---
fprintf('\n[isbanded]\n');
fprintf('  eye(3) (0,0)          -> %d (expect 1)\n', isbanded(eye(3), 0, 0));
fprintf('  tridiag (1,1)         -> %d (expect 1)\n', isbanded([1 2 0; 3 4 5; 0 6 7], 1, 1));
fprintf('  tridiag (0,1)         -> %d (expect 0)\n', isbanded([1 2 0; 3 4 5; 0 6 7], 0, 1));

% --- isdiag / istril / istriu ---
fprintf('\n[isdiag/istril/istriu]\n');
fprintf('  isdiag(diag([1 2 3])) -> %d (expect 1)\n', isdiag(diag([1 2 3])));
fprintf('  isdiag(eye(3))        -> %d (expect 1)\n', isdiag(eye(3)));
fprintf('  istril([1 1; 0 1])    -> %d (expect 0)\n', istril([1 1; 0 1]));
fprintf('  istril([1 0; 1 1])    -> %d (expect 1)\n', istril([1 0; 1 1]));
fprintf('  istriu([1 1; 0 1])    -> %d (expect 1)\n', istriu([1 1; 0 1]));
fprintf('  istriu([1 0; 1 1])    -> %d (expect 0)\n', istriu([1 0; 1 1]));

% --- bandwidth ---
fprintf('\n[bandwidth]\n');
[lo, up] = bandwidth([1 2 0; 3 4 5; 0 6 7]);
fprintf('  tridiag [lo, up]      -> [%d, %d] (expect [1, 1])\n', lo, up);
fprintf('  tridiag lower         -> %d (expect 1)\n', bandwidth([1 2 0; 3 4 5; 0 6 7], 'lower'));
fprintf('  tridiag upper         -> %d (expect 1)\n', bandwidth([1 2 0; 3 4 5; 0 6 7], 'upper'));
fprintf('  upper-tri 1-out       -> %d (expect 0, lower bw)\n', bandwidth([1 2 3; 0 4 5; 0 0 6]));
fprintf('  lower-tri 1-out       -> %d (expect 2, lower bw)\n', bandwidth([1 0 0; 2 3 0; 4 5 6]));

% --- vecnorm ---
fprintf('\n[vecnorm]\n');
fprintf('  [3 4]                 -> %g (expect 5)\n', vecnorm([3 4]));
fprintf('  [3; 4]                -> %g (expect 5)\n', vecnorm([3; 4]));
fprintf('  [3 4; 6 8]            -> '); fprintf('%g ', vecnorm([3 4; 6 8])); fprintf(' (expect 6.7082 8.9443)\n');
fprintf('  [1 2 3 4] p=1         -> %g (expect 10)\n', vecnorm([1 2 3 4], 1));
fprintf('  [1 2 3 4] p=Inf       -> %g (expect 4)\n', vecnorm([1 2 3 4], Inf));
fprintf('  [1 2; 3 4] dim=2      -> '); fprintf('%g ', vecnorm([1 2; 3 4], 2, 2)); fprintf(' (expect 2.2361 5.0)\n');
fprintf('  vecnorm([])           -> %g (expect 0)\n', vecnorm([]));
fprintf('  vecnorm([1 NaN 2])    -> %g (expect NaN)\n', vecnorm([1 NaN 2]));
