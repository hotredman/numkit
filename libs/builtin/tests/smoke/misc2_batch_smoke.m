clear

import compat.*

% Misc batch 2 — string extras + special-fn + helpers. Audit ТЗ closure 2026-05-09.

fprintf('append("ab","cd") = "%s"\n', append("ab","cd"));
fprintf('count("hello","l") = %d\n', count("hello","l"));
fprintf('erase("hello","l") = "%s"\n', erase("hello","l"));
[sn, cn, dn] = ellipj(0.5, 0.5);
fprintf('ellipj(0.5,0.5) = sn=%g cn=%g dn=%g\n', sn, cn, dn);
[K, E] = ellipke(0.5);
fprintf('ellipke(0.5) = K=%g E=%g\n', K, E);
fprintf('erfcx(1) = %g\n', erfcx(1));
fprintf('expint(1) = %g\n', expint(1));
fprintf('flintmax = %g\n', flintmax);
fprintf('cast(3.7,"int32") = %d\n', cast(3.7,"int32"));
fprintf('blkdiag([1 2;3 4],[5 6;7 8]):\n'); disp(blkdiag([1 2;3 4],[5 6;7 8]));
fprintf('feval(@sin, pi/2) = %g\n', feval(@sin, pi/2));
