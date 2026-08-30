clear

fprintf('=== binostat ===\n');
[m, v] = binostat(10, 0.3);
fprintf('  Bin(10,0.3) : m=%g v=%g (expect 3 / 2.1)\n', m, v);
[m, v] = binostat([5 10 20], 0.5);
fprintf('  vec n       : m=[%g %g %g]\n', m(1), m(2), m(3));
[m, v] = binostat(10, 0);
fprintf('  p=0         : m=%g v=%g (expect 0 / 0)\n', m, v);
[m, v] = binostat(10, 1);
fprintf('  p=1         : m=%g v=%g (expect 10 / 0)\n', m, v);
[m, v] = binostat(0, 0.5);
fprintf('  n=0         : m=%g v=%g (expect 0 / 0)\n', m, v);
fprintf('  edges       : n<0 → %g, p<0 → %g, p>1 → %g, n=2.5 → %g (all NaN)\n', binostat(-1,0.5), binostat(10,-0.1), binostat(10,1.5), binostat(2.5,0.5));
