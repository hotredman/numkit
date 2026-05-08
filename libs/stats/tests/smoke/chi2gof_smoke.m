clear;
import compat.*;

% chi2gof — chi-squared goodness-of-fit test.
% Closes audit/findings/stats/chi2gof.md.
%
% New 2026-05-08:
%   - Auto-binning when no Frequency+Expected supplied (default
%     normal CDF fit using mean(x), std(x); 10 bins by default).
%   - 'NBins' / 'Edges' / 'Ctrs' N-V controls.
%   - 'EMin' (default 5) — merges tail bins with small expected
%     counts.
%   - stats struct now populated with edges / O / E (was just
%     chi2stat / df).
%   - 'CDF' function-handle DEFERRED — clear error message.

fprintf('--- explicit Frequency + Expected ---\n');
[h, p, st] = chi2gof((1:5)', 'Frequency', [10 12 8 14 6], ...
                              'Expected', [10 10 10 10 10]);
fprintf('h=%d p=%.10f chi2=%.6f df=%d\n', h, p, st.chi2stat, st.df);
fprintf('edges = '); disp(st.edges);
fprintf('O = '); disp(st.O);
fprintf('E = '); disp(st.E);

fprintf('--- auto-bin with NBins=6 ---\n');
x = (-3:0.05:3)';
[h, p, st] = chi2gof(x, 'NBins', 6);
fprintf('h=%d p=%.10f chi2=%.6f df=%d\n', h, p, st.chi2stat, st.df);
fprintf('edges = '); disp(st.edges);
fprintf('O = '); disp(st.O);

fprintf('--- explicit Edges [-3 -1 0 1 3] ---\n');
[h, p, st] = chi2gof(x, 'Edges', [-3 -1 0 1 3]);
fprintf('h=%d p=%.10f chi2=%.6f df=%d\n', h, p, st.chi2stat, st.df);
fprintf('edges = '); disp(st.edges);
fprintf('O = '); disp(st.O);

fprintf('--- ''CDF'' arg rejected ---\n');
try
    chi2gof(x, 'CDF', 'normcdf');
    fprintf('UNEXPECTED: no error\n');
catch e
    fprintf('OK: %s\n', e.message);
end
