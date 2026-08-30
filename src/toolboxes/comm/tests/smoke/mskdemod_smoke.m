clear

% mskdemod(y, nSamp) — coherent (differential) MSK demodulator, the inverse
% of mskmod. Each bit is the sign of the symbol's accumulated phase
% increment, so the decision is invariant to a constant phase rotation.
% bugs/comm/analog-demodulators.

data = [1 0 1 1 0 0 1 0]; nsamp = 8;
y = mskmod(data, nsamp);
z = mskdemod(y, nsamp);
fprintf('round-trip: data = [%s]\n', sprintf('%d ', data));
fprintf('            z    = [%s]  match = %d  (expect 1)\n', ...
        sprintf('%d ', z), isequal(z(:), data(:)));

% Second output: final phase state (0 here, balanced bit sum).
[~, ph] = mskdemod(y, nsamp);
fprintf('phaseout = %.4f  (expect 0)\n', ph);

% Differential -> robust to a constant phase rotation.
zr = mskdemod(y .* exp(1i*0.7), nsamp);
fprintf('rotated by 0.7 rad: match = %d  (expect 1)\n', isequal(zr(:), data(:)));

% ...and to additive noise.
yn = y + 0.05*(cos(1:numel(y)) + 1i*sin(1:numel(y)));
zn = mskdemod(yn, nsamp);
fprintf('noisy: match = %d  (expect 1)\n', isequal(zn(:), data(:)));
