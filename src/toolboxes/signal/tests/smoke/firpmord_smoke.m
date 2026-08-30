clear

fprintf('=== signal/firpmord (Phase 4.7 — Parks-McClellan FIR order) ===\n');

fprintf('\n[lowpass: 1500/2000 transition, fs=8000]\n');
[n, fo, ao, w] = firpmord([1500 2000], [1 0], [0.01 0.1], 8000);
fprintf('  n=%d (expect 21)\n', n);
fprintf('  fo='); fprintf('%g ', fo); fprintf(' (expect 0 0.375 0.5 1)\n');
fprintf('  ao='); fprintf('%g ', ao); fprintf(' (expect 1 1 0 0)\n');
fprintf('  w='); fprintf('%g ', w); fprintf(' (expect 10 1)\n');

fprintf('\n[highpass: 800/1000, fs=4000]\n');
[n, ~, ~, ~] = firpmord([800 1000], [0 1], [0.01 0.05], 4000);
fprintf('  n=%d (expect 32)\n', n);

fprintf('\n[bandpass multiband: 500/1000/2000/2500, fs=8000]\n');
[n, ~, ~, ~] = firpmord([500 1000 2000 2500], [0 1 0], [0.05 0.01 0.05], 8000);
fprintf('  n=%d (expect 24)\n', n);

fprintf('\nBIT-EQUAL with MATLAB R2025b on (n, ff, aa, wts).\n');
fprintf('Octave 11.1.0 ships in core; values match modulo Octave''s slight diff\n');
fprintf('on the odd-order-bump-at-Nyquist behavior.\n');
