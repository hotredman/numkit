clear
import compat.*

% ddencmp: default denoise/compress thresholds from a 1-level db1 DWT
% noise estimate (sigma = median(|cD1|)/0.6745). bugs/wavelet/ddencmp.

[thr, sorh, keepapp] = ddencmp('den', 'wv', [1 2 3 8 3 2 1 2]);
fprintf('den wv: thr=%.12f sorh=%s keepapp=%d  (expect 2.137919772574 s 1)\n', ...
        thr, sorh, keepapp);

[thr, sorh, keepapp] = ddencmp('cmp', 'wv', [1 2 3 8 3 2 1 2]);
fprintf('cmp wv: thr=%.12f sorh=%s keepapp=%d  (expect 0.707106781187 h 1)\n', ...
        thr, sorh, keepapp);

[thr, sorh] = ddencmp('den', 'wv', [1 2 3 4 5]);
fprintf('den wv [1..5]: thr=%.12f sorh=%s  (expect 1.880854323469 s)\n', thr, sorh);
