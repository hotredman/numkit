import compat.*

% 4 points: 2 close pairs at (0,0)/(0,1) and (10,10)/(10,11)
X = [0 0; 0 1; 10 10; 10 11];
Y = pdist(X);
fprintf('Y = '); disp(Y);
fprintf('  expect: [1 ~14.14 ~14.21 ~14.21 ~14.14 1]\n\n');

Z = linkage(Y);
fprintf('--- linkage(Y, ''single'') ---\n');
disp(Z);
fprintf('  expect: rows like\n');
fprintf('    [1 2 1.0]    (merge points 1,2 at distance 1)\n');
fprintf('    [3 4 1.0]    (merge points 3,4 at distance 1)\n');
fprintf('    [5 6 ~14.14] (merge resulting clusters at ~14.14)\n\n');

% cluster from linkage with maxclust = 2
idx = cluster(Z, 'maxclust', 2);
fprintf('--- cluster(Z, ''maxclust'', 2) ---\n');
disp(idx);
fprintf('  expect: [1; 1; 2; 2] (or some 1/2 permutation)\n\n');

% clusterdata convenience
idx2 = clusterdata(X, 2);
fprintf('--- clusterdata(X, 2) ---\n');
disp(idx2);
fprintf('  expect: same as above\n\n');

% cophenet — correlation between Y and tree distances
co = cophenet(Z, Y);
fprintf('cophenet = %.4f\n', co);
fprintf('  expect: high (≥ 0.9) for clean cluster structure\n\n');

% inconsistent
fprintf('--- inconsistent(Z) ---\n');
disp(inconsistent(Z));
fprintf('  expect: 3×4, last column = inconsistency coefficient\n\n');

% Test complete linkage
Zc = linkage(Y, 'complete');
fprintf('--- linkage(Y, ''complete'') ---\n');
disp(Zc);
fprintf('  expect: similar tree but final distance ≈ 14.21 (max not min)\n\n');

% Test average linkage
Za = linkage(Y, 'average');
fprintf('--- linkage(Y, ''average'') ---\n');
disp(Za);
fprintf('  expect: final distance averaged ≈ 14.18\n');
