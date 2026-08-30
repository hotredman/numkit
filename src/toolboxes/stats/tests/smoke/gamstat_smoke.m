clear

fprintf('=== gamstat ===\n');
[m, v] = gamstat(2, 1);
fprintf('  Gam(2,1) : m=%g v=%g (expect 2 / 2)\n', m, v);
[m, v] = gamstat([2 5 10], [1 2 0.5]);
fprintf('  vec      : m=[%g %g %g] v=[%g %g %g]\n', m(1), m(2), m(3), v(1), v(2), v(3));
fprintf('  edges    : a=0 → %g, b=0 → %g, a<0 → %g (all NaN)\n', gamstat(0,1), gamstat(2,0), gamstat(-1,1));
