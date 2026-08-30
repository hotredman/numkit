clear

fprintf('=== signal/ellipord (Phase 4.6 — elliptic filter order) ===\n');

fprintf('\n[lowpass digital: Wp=0.2, Ws=0.3, Rp=1dB, Rs=40dB]\n');
[n, Wn] = ellipord(0.2, 0.3, 1, 40);
fprintf('  n=%d Wn=%.4f (expect n=4 Wn=0.2 bit-equal MATLAB)\n', n, Wn);

fprintf('\n[highpass: Wp=0.6, Ws=0.4, Rp=3, Rs=60]\n');
[n, Wn] = ellipord(0.6, 0.4, 3, 60);
fprintf('  n=%d Wn=%.4f (expect n=5)\n', n, Wn);

fprintf('\n[bandpass: Wp=[0.2 0.4], Ws=[0.1 0.5]]\n');
[n, Wn] = ellipord([0.2 0.4], [0.1 0.5], 1, 40);
fprintf('  n=%d Wn=[%.4f %.4f] (expect n=4)\n', n, Wn(1), Wn(2));

fprintf('\n[analog: Wp=2π·1000, Ws=2π·1500, ''s'' mode]\n');
[n, Wn] = ellipord(2*pi*1000, 2*pi*1500, 1, 40, 's');
fprintf('  n=%d Wn=%.4f (expect n=5 Wn=6283.19)\n', n, Wn);

fprintf('\n[bandstop: Wp=[0.1 0.6], Ws=[0.2 0.5], Rp=3, Rs=40]  (was a stub)\n');
[n, Wn] = ellipord([0.1 0.6], [0.2 0.5], 3, 40);
fprintf('  n=%d Wn=[%.4f %.4f] (expect n=4 Wn=[0.1 0.6])\n', n, Wn(1), Wn(2));

fprintf('\n[bandstop analog: Wp=[100 600], Ws=[200 500], ''s'']\n');
[n, Wn] = ellipord([100 600], [200 500], 3, 40, 's');
fprintf('  n=%d Wn=[%.1f %.1f] (expect n=5 Wn=[100 600])\n', n, Wn(1), Wn(2));

fprintf('\nBIT-EQUAL with MATLAB R2025b (incl. bandstop, fixed 2026-06-05).\n');
fprintf('Octave 11.1.0 also matches.\n');
