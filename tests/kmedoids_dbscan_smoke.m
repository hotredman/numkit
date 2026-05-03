import compat.*

rng(123);
n = 25;
X = [randn(n, 2); randn(n, 2) + 8; randn(n, 2) + [0 8]];   % 3 clusters

% kmedoids
[idx, M, sumd] = kmedoids(X, 3);
fprintf('--- kmedoids(X, 3) ---\n');
fprintf('cluster sizes: %d, %d, %d\n', sum(idx==1), sum(idx==2), sum(idx==3));
fprintf('  expect: ≈25 / 25 / 25 split\n');
fprintf('Medoids (rows):\n'); disp(M);
fprintf('  expect: 3 rows near (0,0), (8,8), (0,8)\n\n');

% dbscan — same data
[idx2, core] = dbscan(X, 1.5, 4);
fprintf('--- dbscan(X, eps=1.5, minpts=4) ---\n');
fprintf('clusters found: %d\n', max(idx2));
fprintf('noise points:   %d\n', sum(idx2 == 0));
fprintf('core points:    %d / %d\n', sum(double(core)), 75);
fprintf('  expect: ≈3 clusters, some noise at the edges\n\n');

% Specific tiny example with hand-checkable answer
Y = [0 0; 0 0.1; 0 0.2; 5 5; 5 5.1; 5 5.2; 100 100];   % 2 clusters + 1 noise
[idx3, core3] = dbscan(Y, 0.5, 3);
fprintf('--- dbscan tiny: 2 clusters + 1 noise point ---\n');
fprintf('idx (col): '); disp(idx3);
fprintf('core (col): '); disp(double(core3));
fprintf('  expect: idx ≈ [1 1 1 2 2 2 0], core ≈ [1 1 1 1 1 1 0]\n');
