clear

import compat.*

x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';
y = [0.8 1.9 2.7 4.0 4.5 5.7 6.4]';

fprintf('=== signtest with extended stats struct ===\n');
[p, h, st] = signtest(x);
fprintf('  p=%.6f h=%d zval=%g sign=%d\n', p, h, st.zval, st.sign);

fprintf('\n=== signrank ===\n');
[p, h] = signrank(x);
fprintf('  p=%.6f h=%d\n', p, h);

fprintf('\n=== ranksum ===\n');
[p, h] = ranksum(x, y);
fprintf('  p=%.6f h=%d\n', p, h);

fprintf('\n=== fishertest ===\n');
T = [12 5; 4 9];
[h, p, stats] = fishertest(T);
fprintf('  h=%d p=%.6f OR=%.4f\n', h, p, stats.OddsRatio);
