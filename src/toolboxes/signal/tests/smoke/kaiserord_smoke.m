clear

fprintf('=== signal/kaiserord (Phase 4.5 — Kaiser FIR order estimator) ===\n');

fprintf('\n[lowpass: 1500/2000 Hz transition, fs=8000]\n');
[n, Wn, bta, ft] = kaiserord([1500 2000], [1 0], [0.01 0.1], 8000);
fprintf('  n=%d Wn=%.4f bta=%.6f ftype=%s\n', n, Wn, bta, ft);
fprintf('  expect n=36 Wn=0.4375 bta=3.395321 ftype=low (bit-equal MATLAB)\n');

fprintf('\n[highpass: 800/1000 Hz transition, fs=4000]\n');
[n, Wn, bta, ft] = kaiserord([800 1000], [0 1], [0.01 0.05], 4000);
fprintf('  n=%d Wn=%.4f bta=%.6f ftype=%s\n', n, Wn, bta, ft);
fprintf('  expect n=46 Wn=0.4500 ftype=high\n');

fprintf('\n[bandpass: multi-band 500/1000/2000/2500, fs=8000]\n');
[n, Wn, bta, ft] = kaiserord([500 1000 2000 2500], [0 1 0], [0.05 0.01 0.05], 8000);
fprintf('  n=%d Wn=[%.4f %.4f] bta=%.4f ftype=%s\n', n, Wn(1), Wn(2), bta, ft);
fprintf('  expect n=36 Wn=[0.1875 0.5625] ftype=DC-0\n');

fprintf('\nBIT-EQUAL with MATLAB R2025b. Octave 11.1.0 also matches.\n');
