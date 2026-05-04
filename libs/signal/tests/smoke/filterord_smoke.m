clear

import compat.*

% buttord(0.2, 0.3, 1, 30) — typical lowpass spec.
% MATLAB R2025b: N = 11, Wn = 0.2125
[N, Wn] = buttord(0.2, 0.3, 1, 30);
fprintf('buttord(0.2, 0.3, 1, 30):  N = %d, Wn = %.4f\n', N, Wn);

% cheb1ord(0.2, 0.3, 1, 30) — Cheby type I returns Wp as cutoff.
% MATLAB: N = 5, Wn = 0.2
[N1, Wn1] = cheb1ord(0.2, 0.3, 1, 30);
fprintf('cheb1ord(0.2, 0.3, 1, 30): N = %d, Wn = %.4f\n', N1, Wn1);

% cheb2ord(0.2, 0.3, 1, 30) — Cheby type II returns Ws as cutoff.
% MATLAB: N = 5, Wn = 0.3
[N2, Wn2] = cheb2ord(0.2, 0.3, 1, 30);
fprintf('cheb2ord(0.2, 0.3, 1, 30): N = %d, Wn = %.4f\n', N2, Wn2);

% Bandpass spec
% MATLAB buttord([0.2 0.4], [0.1 0.5], 1, 30): N = 9, Wn = [0.2 0.4]
[Nb, Wnb] = buttord([0.2 0.4], [0.1 0.5], 1, 30);
fprintf('buttord([0.2 0.4], [0.1 0.5], 1, 30): N = %d, Wn = [%.4f %.4f]\n', ...
    Nb, Wnb(1), Wnb(2));

% Now verify against MATLAB by designing the filter and checking specs.
[b, a] = butter(N, Wn);
fprintf('butter(N=%d, Wn=%.4f) → b length = %d\n', N, Wn, length(b));
