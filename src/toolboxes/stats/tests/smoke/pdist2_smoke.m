clear;

% pdist2 — pairwise distance between two row sets, with optional
% N-V 'Smallest'/'Largest' k-mode and Mahalanobis cov override.
% gaps #1-#2.

A = [1 2; 3 4; 5 6; 7 8; 9 10];
B = [1 2; 5 5; 9 9];

% --- Default euclidean ---------------------------------------------
D0 = pdist2(A, B);
fprintf('--- D0 = pdist2(A, B) (5x3, euclidean default) ---\n');
disp(D0);
fprintf('expect D0(1,1)=0, D0(5,3)=1, D0(3,2)=1\n\n');

% --- Smallest k -----------------------------------------------------
[Ds, Is] = pdist2(A, B, 'euclidean', 'Smallest', 2);
fprintf('--- [D, I] = pdist2(A, B, ''euclidean'', ''Smallest'', 2) ---\n');
disp('D:'); disp(Ds);
disp('I:'); disp(Is);
fprintf('expect D shape 2x3, D(:,2)=[1; 2.236], I(:,2)=[3; 2]\n\n');

% --- Largest k ------------------------------------------------------
[Dl, Il] = pdist2(A, B, 'euclidean', 'Largest', 2);
fprintf('--- [D, I] = pdist2(A, B, ''euclidean'', ''Largest'', 2) ---\n');
disp('D:'); disp(Dl);
disp('I:'); disp(Il);
fprintf('expect D(:,1)=[11.314; 8.485], I(:,1)=[5; 4]\n\n');

% --- Mahalanobis (default cov(X)) -----------------------------------
X = [0 0; 1 0; 2 2];
Y = [1 0; 0 1; 1 1; -1 -1; 2 -1; -2 1];
Dm = pdist2(X, Y, 'mahalanobis');
fprintf('--- Dm = pdist2(X, Y, ''mahalanobis'') (uses cov(X)) ---\n');
disp(Dm);
fprintf('expect Dm(1,1) ~ 2.0, Dm(2,3) ~ 1.7321\n\n');

% --- Mahalanobis with explicit C ------------------------------------
Dm2 = pdist2(X, Y, 'mahalanobis', cov(Y));
fprintf('--- Dm2 = pdist2(X, Y, ''mahalanobis'', cov(Y)) ---\n');
disp(Dm2);
fprintf('expect Dm2(1,1) ~ 0.7120, Dm2(2,3) ~ 1.066\n');
