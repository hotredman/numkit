clear

% groupfilter — array form, per-group row filtering.
% Reference: MATLAB R2025b.

A = [10; 20; 30; 40; 50; 60]; G = [1; 2; 1; 2; 1; 2];

fprintf('== scalar predicate: keep groups where mean(x)>30 ==\n');
B = groupfilter(A, G, @(x) mean(x) > 30);
fprintf('  %s (e [20;40;60])\n', mat2str(B));

fprintf('\n== vector predicate: keep elements where x>mean(x) ==\n');
B = groupfilter(A, G, @(x) x > mean(x));
fprintf('  %s (e [50;60])\n', mat2str(B));

fprintf('\n== empty result ==\n');
B = groupfilter(A, G, @(x) mean(x) > 1000);
fprintf('  size=[%d %d] (e [0 1])\n', size(B,1), size(B,2));

fprintf('\n== matrix A — row-vec reduce ==\n');
A2 = [10 100; 20 200; 30 300; 40 400; 50 500; 60 600];
B = groupfilter(A2, G, @(x) mean(x) > 30);
fprintf('  size=[%d %d]   rows:\n', size(B,1), size(B,2));
disp(B);

fprintf('\n== NaN group singletons ==\n');
G2 = [1; 2; NaN; 1; 2; NaN];
B = groupfilter(A, G2, @(x) mean(x) > 25);
fprintf('  %s (e [20;30;50;60])\n', mat2str(B));
