clear
import compat.*

% Biorthogonal (bior*) and reverse-biorthogonal (rbio*) wavelet families.
% Unlike haar/db/sym/coif, these have DISTINCT analysis vs synthesis filter
% pairs (Lo_D/Hi_D != Lo_R/Hi_R). bugs/wavelet/dwt-biorthogonal.

% wfilters returns four distinct filters; bior2.2 is len 6.
[LoD, HiD, LoR, HiR] = wfilters('bior2.2');
fprintf('bior2.2 Lo_D(4)=%.10f Lo_R(3)=%.10f  (expect 1.0606601718 0.7071067812)\n', LoD(4), LoR(3));
fprintf('bior2.2 analysis != synthesis: max|Lo_D-Lo_R|=%.4f  (expect ~0.71)\n', max(abs(LoD - LoR)));

% dwt with biorthogonal wavelets.
x = [1 2 3 4 5 6 7 8];
[a, d] = dwt(x, 'bior2.2');
fprintf('dwt bior2.2: a(1)=%.10f a(2)=%.10f  (expect 2.6516504294 1.2374368671)\n', a(1), a(2));
[a4, d4] = dwt(x, 'bior4.4');
fprintf('dwt bior4.4: a(1)=%.10f d(1)=%.10f  (expect 5.6946827050 -0.0645388826)\n', a4(1), d4(1));

% Perfect reconstruction (idwt round-trip).
r = idwt(a, d, 'bior2.2');
fprintf('idwt bior2.2 round-trip err = %.2e  (expect ~0)\n', max(abs(r - x)));

% Multilevel wavedec / waverec.
[C, L] = wavedec(x, 2, 'bior2.2');
fprintf('wavedec bior2.2 C(1)=%.6f L=[%d %d %d %d]  (expect 2.031250 / 5 5 6 8)\n', ...
        C(1), L(1), L(2), L(3), L(4));
xr = waverec(C, L, 'bior2.2');
fprintf('waverec bior2.2 round-trip err = %.2e  (expect ~0)\n', max(abs(xr - x)));

% bior1.1 == Haar.
[ab, db_] = dwt([1 2 3 4], 'bior1.1');
[ah, dh] = dwt([1 2 3 4], 'haar');
fprintf('bior1.1 == haar: max diff = %.2e  (expect ~0)\n', max(abs(ab - ah)));
