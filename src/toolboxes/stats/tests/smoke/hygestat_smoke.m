clear

fprintf('=== hygestat ===\n');
[m, v] = hygestat(50, 20, 10);
fprintf('  Hyge(50,20,10): m=%g v=%.4f (expect 4 / 1.9592)\n', m, v);
[m, v] = hygestat([50 100], 20, 10);
fprintf('  vector M     : m=[%g %g]\n', m(1), m(2));
[m, v] = hygestat(50, 0, 10);
fprintf('  K=0          : m=%g v=%g (no successes)\n', m, v);
[m, v] = hygestat(50, 50, 10);
fprintf('  K=M          : m=%g v=%g (all successes)\n', m, v);
fprintf('  edges        : M=0 → %g, K>M → %g, N>M → %g (NaN)\n', hygestat(0,0,10), hygestat(50,60,10), hygestat(50,20,60));
