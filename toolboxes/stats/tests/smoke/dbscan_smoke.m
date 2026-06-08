clear;
import compat.*;

% dbscan — density-based clustering. Closes
%
%
% Bug fixes 2026-05-08:
%   - noise label was 0, MATLAB R2025b uses -1
%   - 'Distance' N-V keyword was unparsed (only 4th positional worked)
%   - 'Distance', 'precomputed' was not supported
%   - 'P' for Minkowski exponent was not supported

X = [0 0; 0.1 0; 0 0.1; 0.1 0.1; ...
     5 5; 5.1 5; 5 5.1; 5.1 5.1; ...
     10 0; 10.1 0; 10 0.1; ...
     20 20];

fprintf('--- dbscan(X, 0.5, 3) ---\n');
[idx, core] = dbscan(X, 0.5, 3);
fprintf('idx = '); disp(idx');
fprintf('core = '); disp(double(core)');
fprintf('expect [1 1 1 1 2 2 2 2 3 3 3 -1], core mostly 1\n\n');

fprintf('--- dbscan(D, 0.5, 3, ''Distance'', ''precomputed'') ---\n');
D = pdist2(X, X);
disp(dbscan(D, 0.5, 3, 'Distance', 'precomputed')');
fprintf('expect identical to basic\n\n');

fprintf('--- dbscan(X, 0.5, 3, ''Distance'', ''minkowski'', ''P'', 3) ---\n');
disp(dbscan(X, 0.5, 3, 'Distance', 'minkowski', 'P', 3)');
fprintf('expect identical clustering\n\n');

fprintf('--- dbscan(X, 1.0, 3, ''Distance'', ''cityblock'') ---\n');
disp(dbscan(X, 1.0, 3, 'Distance', 'cityblock')');
fprintf('expect [1 1 1 1 2 2 2 2 3 3 3 -1]\n');
