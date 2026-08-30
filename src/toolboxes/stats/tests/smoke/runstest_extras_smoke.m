clear

xs = [-1 1 -1 1 -1 1 -1 1]';
fprintf('=== runstest default (median-based) ===\n');
[h, p, st] = runstest(xs);
fprintf('  h=%d p=%.6f nruns=%d n1=%d n0=%d\n', h, p, st.nruns, st.n1, st.n0);

fprintf('\n=== runstest "ud" (up-down) ===\n');
x = [1.2 2.4 3.1 4.5 5.0 6.2 7.1]';
[h, p, st] = runstest(x, 'ud');
fprintf('  monotonic up: h=%d p=%.6f nruns=%d n1=%d n0=%d\n', ...
    h, p, st.nruns, st.n1, st.n0);

xa = [1 3 2 4 3 5 4 6]';
[h, p, st] = runstest(xa, 'ud');
fprintf('  alternating:  h=%d p=%.6f nruns=%d\n', h, p, st.nruns);
