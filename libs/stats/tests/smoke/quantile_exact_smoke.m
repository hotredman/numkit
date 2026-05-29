clear
import compat.*
% quantile 'Method','exact' (MATLAB default) + 'approximate' — previously rejected.
fprintf('exact 0.25       = %g (expect 1.5)\n', quantile([1 2 3 4], 0.25, 'Method', 'exact'));
fprintf('approximate 0.25 = %g (expect 1.5)\n', quantile([1 2 3 4], 0.25, 'Method', 'approximate'));
fprintf('exact == default : %g vs %g\n', quantile([1:10]', 0.5, 'Method', 'exact'), quantile([1:10]', 0.5));
