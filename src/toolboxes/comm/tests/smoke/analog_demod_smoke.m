clear
import compat.*

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
