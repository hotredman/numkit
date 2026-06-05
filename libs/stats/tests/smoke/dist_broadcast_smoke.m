clear
import compat.*

% bugs/stats/distribution-array-params.md — *pdf/*cdf/*inv broadcast ARRAY
% distribution parameters (not just the data x). Reference: MATLAB R2025b.
% Cycle 29 families: normal + exponential.

fmt = @(v) sprintf('%.8g ', v);

fprintf('--- normal (broadcast mu/sigma) ---\n');
fprintf('normpdf(0,0,[1 2 4])      = %s (expect 0.39894228 0.19947114 0.099735570)\n', fmt(normpdf(0,0,[1 2 4])));
fprintf('normpdf([0 1],[0 0],[1 2])= %s (expect 0.39894228 0.17603266)\n', fmt(normpdf([0 1],[0 0],[1 2])));
fprintf('normcdf(1,[0 1],1)        = %s (expect 0.84134475 0.5)\n', fmt(normcdf(1,[0 1],1)));
fprintf('norminv(0.5,[0 5],1)      = %s (expect 0 5)\n', fmt(norminv(0.5,[0 5],1)));
fprintf('normpdf(0,0,[1 -1 2])     = %s (expect 0.39894228 NaN 0.19947114; sigma<=0 -> NaN)\n', fmt(normpdf(0,0,[1 -1 2])));

fprintf('\n--- exponential (broadcast mu) ---\n');
fprintf('exppdf(1,[1 2 4])         = %s (expect 0.36787944 0.30326533 0.19470020)\n', fmt(exppdf(1,[1 2 4])));
fprintf('expcdf(1,[1 2 4])         = %s (expect 0.63212056 0.39346934 0.22119922)\n', fmt(expcdf(1,[1 2 4])));
fprintf('expinv(0.5,[1 2 4])       = %s (expect 0.69314718 1.3862944 2.7725887)\n', fmt(expinv(0.5,[1 2 4])));
fprintf('expcdf(1,[1 2 4],''upper'') = %s (expect 0.36787944 0.60653066 0.77880078)\n', fmt(expcdf(1,[1 2 4],'upper')));

fprintf('\n--- regressions (scalar-param path unchanged) ---\n');
fprintf('normpdf(0)=%.10g (expect 0.3989422804)  exppdf(2)=%.10g (expect 0.1353352832)\n', normpdf(0), exppdf(2));
fprintf('empty: numel(normpdf([],0,1))=%d (expect 0)\n', numel(normpdf([],0,1)));
