clear

fprintf('=== wblstat ===\n');
[m, v] = wblstat(1, 2);
fprintf('  Wbl(1,2) : m=%.4f v=%.4f\n', m, v);
[m, v] = wblstat([1 2 3], [1 2 3]);
fprintf('  vec      : m=[%g %g %g]\n', m(1), m(2), m(3));
fprintf('  edges    : a=0 → %g, b=0 → %g, a<0 → %g (all NaN)\n', wblstat(0,1), wblstat(1,0), wblstat(-1,1));
