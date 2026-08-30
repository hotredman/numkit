clear

% 2-D multilevel wavelet decomposition: wavedec2 / detcoef2 / appcoef2 /
% waverec2. Iterates the single-level dwt2 on the LL band; [C,S] is the
% coarsest-first coefficient vector + size bookkeeping matrix.
% bugs/wavelet/wavedec2-family.

% Single-level db1 (Haar) on a 4x4 ramp.
X = reshape(1:16, 4, 4);
[c, s] = wavedec2(X, 1, 'db1');
fprintf('wavedec2 db1 4x4: numel(c)=%d  s(1,:)=[%d %d]  c(1)=%.4f  (expect 16 / 2 2 / 7)\n', ...
        numel(c), s(1,1), s(1,2), c(1));

H = detcoef2('h', c, s, 1);
V = detcoef2('v', c, s, 1);
D = detcoef2('d', c, s, 1);
fprintf('detcoef2 h/v/d L1: H(1,1)=%.1f V(1,1)=%.1f D(1,1)=%.1f  (expect -1 / -4 / 0)\n', ...
        H(1,1), V(1,1), D(1,1));

A = appcoef2(c, s, 'db1', 1);
fprintf('appcoef2 L1: A(1,1)=%.1f A(2,2)=%.1f  (expect 7 / 27)\n', A(1,1), A(2,2));

% Two-level db2 on an 8x8 ramp.
Y = reshape(1:64, 8, 8);
[c2, s2] = wavedec2(Y, 2, 'db2');
fprintf('wavedec2 db2 8x8 N=2: numel(c)=%d  (expect 139)\n', numel(c2));
A2 = appcoef2(c2, s2, 'db2', 2);
fprintf('appcoef2 L2: A(1,1)=%.10f size=[%d %d]  (expect 16.4557713660 / 4 4)\n', ...
        A2(1,1), size(A2,1), size(A2,2));
R = waverec2(c2, s2, 'db2');
fprintf('waverec2 round-trip err = %.2e  (expect ~0)\n', max(max(abs(R - Y))));

% Biorthogonal wavelet in 2-D (distinct analysis/synthesis filters).
[cb, sb] = wavedec2(X, 1, 'bior2.2');
Rb = waverec2(cb, sb, 'bior2.2');
fprintf('bior2.2 2-D round-trip err = %.2e  (expect ~0)\n', max(max(abs(Rb - X))));
