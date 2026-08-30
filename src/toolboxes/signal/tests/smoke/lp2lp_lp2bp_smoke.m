clear

% lp2lp / lp2bp numerator length + tf2zp gain (DEEP-PROBE 2026-05-31).
% MATLAB's lp2* TF form returns the numerator at its TRUE degree
% (#zeros + 1), not zero-padded to the denominator length. The root cause
% of numkit's earlier mismatch was tf2zp computing the ZPK gain from b(1)
% (= 0 for a zero-padded numerator like [0 0 0 0 1]) instead of the first
% nonzero coefficient. vs MATLAB R2025b.

fprintf('=== tf2zp gain skips leading zeros ===\n');
[z2, p2, k2] = tf2zp([0 0 0 0 1], [1 2 3 4 5]);
fprintf('tf2zp([0 0 0 0 1]): numel(z)=%d k=%g  (expect 0, 1)\n', numel(z2), k2);
[z3, p3, k3] = tf2zp([0 0 3 33 90], [1 6 11 6 1]);
fprintf('tf2zp([0 0 3 33 90]): numel(z)=%d k=%g  (expect 2, 3)\n', numel(z3), k3);

fprintf('\n=== lp2lp (4th-order Butterworth, no zeros) ===\n');
[z, p, k] = buttap(4); [bp, ap] = zp2tf(z, p, k);
[bt, at] = lp2lp(bp, ap, 100);
fprintf('numel(bt)=%d numel(at)=%d  (expect 1, 5 -- NOT 5, 5)\n', numel(bt), numel(at));
fprintf('bt(1)=%.6g  (expect 1e8 = Wo^4)\n', bt(1));
fprintf('at(2)=%.9g at(5)=%.6g  (expect 261.312593, 1e8)\n', at(2), at(5));

fprintf('\n=== lp2bp ===\n');
[bb, ab] = lp2bp(bp, ap, 100, 50);
fprintf('numel(bb)=%d numel(ab)=%d  (expect 5, 9 -- NOT 9, 9)\n', numel(bb), numel(ab));
fprintf('bb(1)=%.6g ab(9)=%.6g  (expect 6.25e6, 1e16 = Wo^8)\n', bb(1), ab(9));

fprintf('\n=== lp2hp / lp2bs unchanged (numerator already full length) ===\n');
[bh, ah] = lp2hp(bp, ap, 100);
[bs, as] = lp2bs(bp, ap, 100, 50);
fprintf('lp2hp numel sum=%d (expect 10); lp2bs numel sum=%d (expect 18)\n', ...
        numel(bh)+numel(ah), numel(bs)+numel(as));
