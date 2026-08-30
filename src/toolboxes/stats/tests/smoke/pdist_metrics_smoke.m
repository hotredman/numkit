clear

% pdist 'seuclidean' / 'spearman' metrics + cosine/correlation NaN edge.
% Fixed 2026-06-05 (bugs/stats/pdist-metrics.md). Reference: MATLAB R2025b.

A = [1 2 3; 4 5 7; 1 0 2];

ds = pdist(A, 'seuclidean');
fprintf('seuclidean default : %.5f %.5f %.5f  (expect 2.58974 0.88002 3.24327)\n', ds(1), ds(2), ds(3));

dse = pdist(A, 'seuclidean', [1 2 3]);
fprintf('seuclidean S=[1 2 3]: %.5f %.5f %.5f  (expect 3.60940 1.05409 4.24591)\n', dse(1), dse(2), dse(3));

dsp = pdist(A, 'spearman');
fprintf('spearman (no ties) : %.4f %.4f %.4f  (expect ~0 0.5 0.5)\n', dsp(1), dsp(2), dsp(3));

dt = pdist([1 1 2; 3 2 2], 'spearman');
fprintf('spearman (ties)    : %.4f  (expect 1.5)\n', dt(1));

cn = pdist([0 0; 3 4], 'cosine');
fprintf('cosine zero-row    : isnan=%d  (expect 1 -> NaN, was 1.0)\n', isnan(cn));

rn = pdist([1 1; 3 4], 'correlation');
fprintf('correlation const  : isnan=%d  (expect 1 -> NaN)\n', isnan(rn));

P2 = pdist2([1 2 3; 4 5 7], [1 0 2; 2 2 2], 'seuclidean');
fprintf('pdist2 seuclidean  : %.5f %.5f  (expect 1.00692 3.26811)\n', P2(1,1), P2(2,1));

Q2 = pdist2([1 2 3; 4 5 7], [1 0 2; 2 2 2], 'spearman');
fprintf('pdist2 spearman    : %.4f isnan(col2)=%d  (expect 0.5 1)\n', Q2(1,1), isnan(Q2(1,2)));
