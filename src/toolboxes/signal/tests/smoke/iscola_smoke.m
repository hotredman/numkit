clear

% signal/iscola — Constant OverLap-Add compliance check.
% Reference: MATLAB R2025b.

fprintf('=== iscola (COLA compliance) ===\n');

w = hann(64, 'periodic');
[tf, m, dev] = iscola(w, 32, 'ola');
fprintf('  hann(64) @ 50%% ola:  tf=%d, m=%g, dev=%g  (e 1, 1.0, ~2e-16)\n', ...
        tf, m, dev);

[tf, m, dev] = iscola(w, 32, 'wola');
fprintf('  hann(64) @ 50%% wola: tf=%d, m=%g, dev=%g  (e 0, 0.75, 0.25)\n', ...
        tf, m, dev);

% Default method is 'wola'.
[tf, m] = iscola(w, 32);
fprintf('  hann(64) @ 50%% default: tf=%d, m=%g  (default=wola)\n', tf, m);

w2 = hamming(64, 'periodic');
[tf, m] = iscola(w2, 32, 'ola');
fprintf('  hamming(64) @ 50%% ola: tf=%d, m=%g  (e 1, 1.08)\n', tf, m);

wr = ones(1, 64);
[tf, m] = iscola(wr, 0, 'ola');
fprintf('  rect(64) @ no-overlap: tf=%d, m=%g  (e 1, 1.0)\n', tf, m);

[tf, m, dev] = iscola(w, 31, 'ola');
fprintf('  hann(64) odd hop=33:  tf=%d, dev=%g  (e 0, ~0.033)\n', tf, dev);

[tf, m] = iscola(w, 48, 'ola');
fprintf('  hann(64) @ 75%% ola:  tf=%d, m=%g  (e 1, 2.0)\n', tf, m);

% Tolerance is relative: |m| · eps.
fprintf('\nBit-equal MATLAB R2025b. Tolerance: maxDev ≤ |m| · eps.\n');
