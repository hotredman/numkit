% toolboxes/linalg/tests/smoke/svd_complex_smoke.m
%
% Smoke demo for complex SVD, rank, and pinv.

disp('--- Complex SVD & Pinv Smoke ---');
B = [1+1i, 2; 3, 4-1i];

[U, S, V] = svd(B);
disp('SVD residual U * S * V'' - B:');
disp(max(max(abs(U*S*V' - B))));

disp('Rank:');
r = rank(B);
disp(r);

disp('Pseudo-inverse pinv(B):');
P = pinv(B);
disp('B * P * B - B residual:');
disp(max(max(abs(B*P*B - B))));
