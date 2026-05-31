clear
import compat.*

% The 4th output of the parametric hypothesis tests is now MATLAB's stats
% struct (was a bare scalar test statistic):
%   ttest / ttest2 -> {tstat, df, sd}   (ttest2 sd: pooled for 'equal',
%                                          [std(x) std(y)] for 'unequal')
%   vartest        -> {chisqstat, df}
%   vartest2       -> {fstat, df1, df2}
% ztest's 4th output is the bare zval scalar (MATLAB convention) — unchanged.

x = 1:10;
y = [2 3 4 5 6 8 9 11 13 15];

[~, ~, ~, s1] = ttest(x, 5);
fprintf('ttest    tstat=%.6f df=%g sd=%.6f   (expect 0.522233, 9, 3.027650)\n', s1.tstat, s1.df, s1.sd);

[~, ~, ~, s2] = ttest2(x, y);
fprintf('ttest2   tstat=%.6f df=%g sd=%.6f   (expect -1.247831, 18, 3.763125; equal default)\n', s2.tstat, s2.df, s2.sd);

[~, ~, ~, s4] = vartest(x, 5);
fprintf('vartest  chisqstat=%.6f df=%g   (expect 16.5, 9)\n', s4.chisqstat, s4.df);

[~, ~, ~, s5] = vartest2(x, y);
fprintf('vartest2 fstat=%.6f df1=%g df2=%g   (expect 0.478538, 9, 9)\n', s5.fstat, s5.df1, s5.df2);

[~, ~, ~, zv] = ztest(x, 5, 2);
fprintf('ztest    zval=%.6f isstruct=%d   (expect 0.790569, 0)\n', zv, isstruct(zv));
