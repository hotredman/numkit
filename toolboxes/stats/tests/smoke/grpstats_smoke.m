clear
import compat.*

fprintf('=== grpstats (per-group statistics) ===\n');

X = [1 10; 2 20; 3 30; 4 40; 5 50; 6 60];
g = [1 1 2 2 1 2];

m = grpstats(X, g);
fprintf('  default (mean):\n');
disp(m)
fprintf('  expect:\n    2.6667   26.6667\n    4.3333   43.3333\n');

[m2, s2] = grpstats(X, g, {'mean', 'std'});
fprintf('\n  multi-fn {mean, std}:\n');
fprintf('  std:\n');
disp(s2)
fprintf('  expect:\n    2.0817   20.8167\n    1.5275   15.2753\n');

ms = grpstats(X, g, 'sum');
fprintf('\n  sum:\n');
disp(ms)
fprintf('  expect:\n    8     80\n   13    130\n');

mn = grpstats(X, g, 'numel');
fprintf('\n  numel:\n');
disp(mn)
fprintf('  expect:\n    3     3\n    3     3\n');

% Vector input
v = [1 2 3 4 5 6];
mv = grpstats(v, g);
fprintf('\n  vector input mean: ');
fprintf('%g ', mv);
fprintf('  (expect 2.6667 4.3333)\n');
fprintf('  shape: [%d %d] (expect [2 1])\n', size(mv,1), size(mv,2));

% NaN exclusion
xn = [1 NaN; 2 20; 3 30];
gn = [1 1 1];
mn2 = grpstats(xn, gn);
fprintf('\n  NaN excluded: '); fprintf('%g ', mn2); fprintf('  (expect 2 25)\n');
