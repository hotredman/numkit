clear
import compat.*

% corner(I) — detect corner points by wrapping cornermetric: local maxima
% above QualityLevel*max, strength-sorted, up to N [x y]=[col row] coords.
% bugs/image/corner.

% A bright square block has 4 corners (returned column-major).
I = zeros(20,20); I(6:15,6:15) = 1;
C = corner(I);
fprintf('square: %d corners  (expect 4)\n', size(C,1));
fprintf('  first=[%d %d] last=[%d %d]  (expect [6 6] / [15 15])\n', ...
        C(1,1), C(1,2), C(end,1), C(end,2));

% Two squares, different contrast: strong corners sort first.
A = zeros(30,30); A(5:9,5:9) = 1; A(20:27,20:27) = 0.5;
Ca = corner(A);
fprintf('two squares: %d corners  (expect 8, strong first)\n', size(Ca,1));
fprintf('  Ca(1,:)=[%d %d] (strong) Ca(5,:)=[%d %d] (weak)\n', ...
        Ca(1,1), Ca(1,2), Ca(5,1), Ca(5,2));

% Strength beats position: strong block at high columns, N=1.
W = zeros(30,30); W(5:9,5:9) = 0.3; W(20:24,20:24) = 1.0;
Cw = corner(W, 1);
fprintf('strong-at-high-col, N=1: [%d %d]  (expect [20 20])\n', Cw(1,1), Cw(1,2));

% Block at the image edge: border corners excluded, only the inner one.
B = zeros(20,20); B(1:6,1:6) = 1;
Cb = corner(B);
fprintf('edge block: %d corner(s)  (expect 1, [%d %d] = [6 6])\n', ...
        size(Cb,1), Cb(1,1), Cb(1,2));

% MinimumEigenvalue (Shi-Tomasi) method.
Cme = corner(I, 'MinimumEigenvalue');
fprintf('MinimumEigenvalue: %d corners  (expect 4)\n', size(Cme,1));
