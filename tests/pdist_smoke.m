import compat.*

% pdist on 4 points in 2-D
X = [0 0; 3 0; 0 4; 3 4];
% Pairwise distances:
% (1,2)=3, (1,3)=4, (1,4)=5, (2,3)=5, (2,4)=4, (3,4)=3
fprintf('--- pdist(X) ---\n');
disp(pdist(X));
fprintf('  expect: [3 4 5 5 4 3]\n\n');

fprintf('--- pdist(X, ''cityblock'') ---\n');
disp(pdist(X, 'cityblock'));
fprintf('  expect: [3 4 7 7 4 3]\n\n');

fprintf('--- pdist(X, ''chebychev'') ---\n');
disp(pdist(X, 'chebychev'));
fprintf('  expect: [3 4 4 4 4 3]\n\n');

fprintf('--- pdist(X, ''minkowski'', 3) ---\n');
disp(pdist(X, 'minkowski', 3));
fprintf('  expect: [3 4 ~5.0397 ~5.0397 4 3]\n\n');

% pdist2 — distance from each X row to each Y row
Y = [1 1; 2 2];
fprintf('--- pdist2(X, Y) ---\n');
disp(pdist2(X, Y));
fprintf('  expect: [√2 ~2.83; √10 √5; √10 √5; √13 √5]\n\n');

% squareform round-trip
fprintf('--- squareform(pdist(X)) ---\n');
D = squareform(pdist(X));
disp(D);
fprintf('  expect: 4×4 symmetric distance matrix, zero diagonal\n\n');

fprintf('--- squareform(D) back ---\n');
disp(squareform(D));
fprintf('  expect: same as pdist(X) [3 4 5 5 4 3]\n\n');

% mahal — distance from origin to mean of X
mu = mean(X);
fprintf('--- mahal(mu, X) (mean-to-mean = 0) ---\n');
disp(mahal(mu, X));
fprintf('  expect: 0 (identity case)\n\n');

% mahal on far point
fprintf('--- mahal([100 100], X) ---\n');
disp(mahal([100 100], X));
fprintf('  expect: large positive number\n');
