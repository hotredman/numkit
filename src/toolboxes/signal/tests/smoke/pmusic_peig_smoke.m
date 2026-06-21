clear
import compat.*

% MUSIC / eigenvector pseudospectra -- bugs/signal/pmusic-peig.
% Subspace frequency estimators: build R = X'X (order 2p), eigendecompose, use
% the noise subspace to sharpen peaks at the signal frequencies. Validated by
% the PEAK LOCATIONS (the absolute pseudospectrum is scale-arbitrary).

n = 0:63;
x = cos(2*pi*0.1*n) + cos(2*pi*0.25*n);   % tones at 0.1 and 0.25 cyc/sample

[P, f] = pmusic(x, 4);
fprintf('pmusic: numel(P)=%d  f=[%.4g .. %.4g]   (expect 129, 0..3.1416)\n', numel(P), f(1), f(end));
fa = f(find(P == max(P(f < 1)), 1));
fb = f(find(P == max(P(f > 1 & f < 2)), 1));
fprintf('  peaks at f = %.4f, %.4f   (expect 0.6381, 1.5708 rad/sample)\n', fa, fb);

[Pe, fe] = peig(x, 4);
ga = fe(find(Pe == max(Pe(fe < 1)), 1));
gb = fe(find(Pe == max(Pe(fe > 1 & fe < 2)), 1));
fprintf('peig peaks at f = %.4f, %.4f   (expect 0.6381, 1.5708)\n', ga, gb);
