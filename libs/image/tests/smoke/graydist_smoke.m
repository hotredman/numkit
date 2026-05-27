clear
import compat.*

% graydist — gray-weighted geodesic distance transform.

A = [1 2 3 4; 2 11 12 2; 3 13 14 3; 4 15 16 4];
seed = false(4,4); seed(1,1) = true;

fprintf('=== cityblock ===\n');
D = graydist(A, seed, 'cityblock');
fprintf('D(2,2)=%.4f D(3,3)=%.4f D(4,4)=%.4f (expect 8 21.5 16.5)\n', ...
    D(2,2), D(3,3), D(4,4));

fprintf('\n=== chessboard (default) ===\n');
D = graydist(A, seed, 'chessboard');
fprintf('D(2,2)=%.4f D(3,3)=%.4f (expect 6 14.5)\n', D(2,2), D(3,3));

fprintf('\n=== quasi-euclidean ===\n');
D = graydist(A, seed, 'quasi-euclidean');
fprintf('D(3,3)=%.6f (expect 18.535534)\n', D(3,3));

fprintf('\n=== (C, R) form ===\n');
D = graydist(A, 1, 1, 'cityblock');
fprintf('D(2,2)=%.4f (expect 8 — same as mask)\n', D(2,2));

fprintf('\n=== IND form ===\n');
D = graydist(A, 1, 'cityblock');
fprintf('D(2,2)=%.4f (expect 8)\n', D(2,2));

fprintf('\n=== multi-seed ===\n');
s2 = false(4,4); s2(1,1)=true; s2(4,4)=true;
D = graydist(A, s2, 'cityblock');
fprintf('D(3,3)=%.4f D(4,3)=%.4f (expect 12 10)\n', D(3,3), D(4,3));

fprintf('\n=== uint8 ===\n');
D = graydist(uint8(A), seed, 'cityblock');
fprintf('D(2,2)=%.4f class=%s (expect 8 single)\n', D(2,2), class(D));
