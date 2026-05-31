clear
import compat.*
% histcounts(...,'BinMethod','integers') makes one unit-width bin centered on
% each integer in [round(min), round(max)]; edges are center +/- 0.5.
% DEEP-PROBE 2026-05-31: 'BinMethod' was rejected outright.

[n, e] = histcounts([1 1 2 3 3 3], 'BinMethod', 'integers');
fprintf('A integers: nbins=%d (expect 3)\n', numel(n));
fprintf('  edges = [%g %g %g %g] (expect 0.5 1.5 2.5 3.5)\n', e(1), e(2), e(3), e(4));
fprintf('  counts = [%g %g %g] (expect 2 1 3)\n', n(1), n(2), n(3));

[m, em] = histcounts([2 5 5 7], 'BinMethod', 'integers');
fprintf('B gaps: nbins=%d (expect 6) e1=%g eend=%g (expect 1.5 7.5)\n', numel(m), em(1), em(end));
fprintf('  counts(1)=%g (expect 1) counts(4)=%g (expect 2) counts(6)=%g (expect 1)\n', m(1), m(4), m(6));

[d, ed] = histcounts([1.2 2.8 2.9 3.1], 'BinMethod', 'integers');
fprintf('C non-integer: nbins=%d (expect 3) e1=%g eend=%g (expect 0.5 3.5) d3=%g (expect 3)\n', ...
        numel(d), ed(1), ed(end), d(3));

[k, ek] = histcounts([-2 -1 -1 0], 'BinMethod', 'integers');
fprintf('D negatives: e1=%g eend=%g (expect -2.5 0.5) counts=[%g %g %g] (expect 1 2 1)\n', ...
        ek(1), ek(end), k(1), k(2), k(3));

p = histcounts([1 1 2 3], 'BinMethod', 'integers', 'Normalization', 'probability');
fprintf('E probability: p1=%g (expect 0.5) p2=%g (expect 0.25)\n', p(1), p(2));
