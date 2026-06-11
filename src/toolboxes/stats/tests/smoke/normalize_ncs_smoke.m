clear
import compat.*

% normalize [N, C, S]: the centering (C) and scaling (S) values, such that
% N == (A - C) ./ S. One C/S value per operating slice — a scalar for a
% vector, a 1-by-W row for a matrix (column-wise). MATLAB R2025b.

[~, c, s] = normalize([2 4 6]);
fprintf('zscore  C=%g S=%g  (expect 4, 2 = mean, std)\n', c, s);
[~, c, s] = normalize([2 4 6], 'range');
fprintf('range   C=%g S=%g  (expect 2, 4 = min, max-min)\n', c, s);
[~, c, s] = normalize([3 4], 'norm');
fprintf('norm    C=%g S=%g  (expect 0, 5 = 0, vecnorm)\n', c, s);
[~, c, s] = normalize([2 4 6], 'center');
fprintf('center  C=%g S=%g  (expect 4, 1)\n', c, s);
[~, c, s] = normalize([2 4 6], 'scale');
fprintf('scale   C=%g S=%g  (expect 0, 2)\n', c, s);

% matrix: column-wise C / S (1-by-3)
[~, cm, sm] = normalize([1 2 3; 4 5 6]);
fprintf('matrix  Csz=%dx%d  C=[%s]  S=[%s]\n', size(cm,1), size(cm,2), num2str(cm), num2str(sm));
fprintf('        (expect 1x3, [2.5 3.5 4.5], [2.1213 2.1213 2.1213])\n');

% identity: N == (A - C) ./ S
A = [2 4 6];
[N, c, s] = normalize(A);
fprintf('identity N==(A-C)./S : %d (expect 1)\n', max(abs(N - (A - c) ./ s)) < 1e-12);

% single-output path unchanged
only = normalize([1 2 3 4 5]);
fprintf('1-output only(1)=%.4f (expect -1.2649)\n', only(1));
