clear
import compat.*

% Exact Kolmogorov-Smirnov p-value + critical value. Fixed 2026-06-05
% (bugs/stats/kstest-pvalue.md). Reference: MATLAB R2025b.

x = [-1 0 1 2 -0.5 0.5];
[h, p, ks, cv] = kstest(x);
fprintf('kstest two-sided: ks=%.7f p=%.8g cv=%.6g  (expect 0.1914625 0.94998410 0.51926)\n', ks, p, cv);

[~, pl] = kstest(x, 'Tail', 'larger');
[~, ps] = kstest(x, 'Tail', 'smaller');
fprintf('  one-sided: larger p=%.8g (expect 0.97197377)  smaller p=%.8g (expect 0.57170523)\n', pl, ps);

[~,~,~,c10] = kstest(x, 'Alpha', 0.10);
[~,~,~,c01] = kstest(x, 'Alpha', 0.01);
fprintf('  cv(0.10)=%.6g cv(0.01)=%.6g  (expect 0.46799 0.61661)\n', c10, c01);

[h2, p2, ks2] = kstest2([1 2 3 4 5], [2 3 4 5 6 7]);
fprintf('kstest2 two-sided: ks=%.6f p=%.8g  (expect 0.333333 0.84705434)\n', ks2, p2);
[~, p2l] = kstest2([1 2 3 4 5], [2 3 4 5 6 7], 'Tail', 'larger');
fprintf('  larger p=%.8g  (expect 0.47200535)\n', p2l);
