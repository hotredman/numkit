clear

fprintf('=== Audio spectral shape descriptors (cycle B) ===\n');
fprintf('NOTE: input form (X, F) — direct power-spectrum mode\n');
fprintf('      input form (x, fs) — internal STFT (rectwin 30ms, overlap 20ms)\n\n');

% Direct (X, F) form — easiest to verify by hand
X = [4; 3; 2; 1];
F = [100; 200; 300; 400];

fprintf('[direct (X, F) form, X=[4;3;2;1], F=[100;200;300;400]]\n');
fprintf('  spectralCentroid = %g (expect 200 = (4*100+3*200+2*300+1*400)/10)\n', spectralCentroid(X, F));
fprintf('  spectralSpread   = %g (expect 100)\n', spectralSpread(X, F));
fprintf('  spectralRolloff  = %g (expect 400, 95%% of energy)\n', spectralRolloffPoint(X, F));
fprintf('  spectralDecrease = %g (expect -0.5)\n', spectralDecrease(X, F));
fprintf('  spectralSlope    = %g (expect -0.01)\n', spectralSlope(X, F));

fprintf('\n[two-column matrix form]\n');
X2 = [4 1; 3 2; 2 3; 1 4];
F2 = [100; 200; 300; 400];
sc = spectralCentroid(X2, F2);
fprintf('  Centroid: '); fprintf('%g ', sc); fprintf(' (expect 200 300)\n');
sf = spectralFlux(X2, F2);
fprintf('  Flux: '); fprintf('%g ', sf); fprintf(' (expect 0 4.4721 — first frame = 0 MATLAB convention)\n');

fprintf('\n[time-domain form (x, fs) — STFT internally]\n');
fs = 8000;
t = (0:1/fs:0.05)';   % 0.05s = 401 samples
x = sin(2*pi*440*t);
sc_t = spectralCentroid(x, fs);
fprintf('  spectralCentroid(440Hz sine, fs=8000) numel=%d first=%.2f\n', numel(sc_t), sc_t(1));
fprintf('  (expect ~440 — fundamental)\n');
