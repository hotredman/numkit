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

fprintf('\n--- gamma (broadcast a/b) ---\n');
fprintf('gampdf(1,[1 2 3],1)       = %s (expect 0.36787944 0.36787944 0.18393972)\n', fmt(gampdf(1,[1 2 3],1)));
fprintf('gamcdf([1 2 3],2,2)       = %s (expect 0.090204010 0.26424112 0.44217460)\n', fmt(gamcdf([1 2 3],2,2)));
fprintf('gampdf(1,[0 -1 2],1)      = %s (expect 0 NaN 0.36787944; a==0->0, a<0->NaN)\n', fmt(gampdf(1,[0 -1 2],1)));

fprintf('\n--- beta (broadcast a/b) ---\n');
fprintf('betapdf(0.5,[2 3],[2 2])  = %s (expect 1.5 1.5)\n', fmt(betapdf(0.5,[2 3],[2 2])));
fprintf('betacdf(0.5,[2 3],2)      = %s (expect 0.5 0.3125)\n', fmt(betacdf(0.5,[2 3],2)));

fprintf('\n--- chi2 (broadcast k) ---\n');
fprintf('chi2pdf(2,[1 2 3])        = %s (expect 0.10377687 0.18393972 0.20755375)\n', fmt(chi2pdf(2,[1 2 3])));
fprintf('chi2cdf(2,[1 2 3])        = %s (expect 0.84270079 0.63212056 0.42759330)\n', fmt(chi2cdf(2,[1 2 3])));
fprintf('chi2pdf(2,[0 -1 4])       = %s (expect 0 NaN 0.18393972; k==0->0, k<0->NaN)\n', fmt(chi2pdf(2,[0 -1 4])));

fprintf('\n--- inverse (broadcast a/b/k) ---\n');
fprintf('gaminv(0.5,[1 2 3],1)     = %s (expect 0.69314718 1.6783470 2.6740603)\n', fmt(gaminv(0.5,[1 2 3],1)));
fprintf('gaminv(0.5,[0 2],1)       = %s (expect 0 1.6783470; a==0->0)\n', fmt(gaminv(0.5,[0 2],1)));
fprintf('betainv([.1 .5 .9],2,3)   = %s (expect 0.14255932 0.38572757 0.67953942)\n', fmt(betainv([.1 .5 .9],2,3)));
fprintf('chi2inv(0.5,[1 2 3])      = %s (expect 0.45493642 1.3862944 2.3659739)\n', fmt(chi2inv(0.5,[1 2 3])));
fprintf('chi2inv(0.5,[0 4])        = %s (expect 0 3.3566940; k==0->0)\n', fmt(chi2inv(0.5,[0 4])));

fprintf('\n--- regressions (scalar-param path unchanged) ---\n');
fprintf('normpdf(0)=%.10g (expect 0.3989422804)  exppdf(2)=%.10g (expect 0.1353352832)\n', normpdf(0), exppdf(2));
fprintf('gampdf(1,2,2)=%.10g (expect 0.1516326649)  betapdf(0.3,2,3)=%.10g (expect 1.764)\n', gampdf(1,2,2), betapdf(0.3,2,3));
fprintf('gaminv(0.5,2,2)=%.10g (expect 3.356693980)  chi2inv(0.5,4)=%.10g (expect 3.356693980)\n', gaminv(0.5,2,2), chi2inv(0.5,4));
fprintf('empty: numel(normpdf([],0,1))=%d (expect 0)\n', numel(normpdf([],0,1)));
