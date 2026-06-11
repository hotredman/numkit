clear

import compat.*

xg = [3 5 4 7 8 6 9 10 11]';
g  = [1 1 1 2 2 2 3 3 3]';
[p, tbl, st] = kruskalwallis(xg, g, 'off');
fprintf('p = %.6f, chi2 = %g, df = %g\n', p, st.chi2stat, st.df);
fprintf('n         = %g %g %g\n', st.n(1), st.n(2), st.n(3));
fprintf('meanranks = %g %g %g\n', st.meanranks(1), st.meanranks(2), st.meanranks(3));

fprintf('\nmatrix-only form:\n');
M = [3 7 9; 5 8 10; 4 6 11];
[p, ~, st] = kruskalwallis(M, [], 'off');
fprintf('  p = %.6f, n = [%g %g %g]\n', p, st.n(1), st.n(2), st.n(3));
