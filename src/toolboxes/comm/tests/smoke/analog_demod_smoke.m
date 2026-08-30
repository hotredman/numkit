clear

% Analog phase/frequency demodulators (inverses of pmmod/fmmod).
% bugs/comm/analog-demodulators. Depend on the length-N hilbert fix.

fs = 100; fc = 10; t = (0:fs-1)'/fs; m = cos(2*pi*1*t);

mp = pmdemod(pmmod(m, fc, fs, 2), fc, fs, 2);
fprintf('pmdemod: mp(1:3)=[%s]  recover err=%.2e  (expect ~[1 0.998 0.992])\n', ...
        num2str(mp(1:3)', '%.6f '), max(abs(mp - m)));

mf = fmdemod(fmmod(m, fc, fs, 5), fc, fs, 5);
fprintf('fmdemod: mf(1:3)=[%s]  (expect [0 0.99774 0.99189])\n', ...
        num2str(mf(1:3)', '%.6f '));
% interior recovery (skip the diff edge at sample 1)
fprintf('fmdemod interior err=%.2e\n', max(abs(mf(2:end) - m(2:end))));

% amdemod / ssbdemod: coherent detection 2*filtfilt(butter(5,fc*2/fs), y.*cos).
t2 = (0:199)'/fs; m2 = cos(2*pi*1*t2);
ma = amdemod(ammod(m2, fc, fs), fc, fs);
ms = ssbdemod(ssbmod(m2, fc, fs), fc, fs);
fprintf('amdemod ma(100)=%.10f  (expect 0.9981621945)\n', ma(100));
fprintf('ssbdemod ms(100)=%.10f  (expect 0.9982563125)\n', ms(100));
fprintf('am interior recover err=%.2e\n', max(abs(ma(60:140) - m2(60:140))));
