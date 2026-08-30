clear

fprintf('=== unidinv ===\n');
fprintf('  median (N=6)    : %g (expect 3)\n', unidinv(0.5, 6));
fprintf('  tails (N=6)     : low=%g, hi=%g (expect 1, 6)\n', unidinv(0.1, 6), unidinv(0.99, 6));
v = unidinv([0.05 0.5 0.95], 10);
fprintf('  vec p (N=10)    : [%g %g %g]\n', v(1), v(2), v(3));
fprintf('  p=0 → %g (NaN — no integer pre-image), p=1 → %g (N=6)\n', ...
    unidinv(0, 6), unidinv(1, 6));
fprintf('  edges: p<0 → %g, p>1 → %g, N=0 → %g, N=-1 → %g, N=6.5 → %g (NaN)\n', ...
    unidinv(-0.1, 6), unidinv(1.5, 6), unidinv(0.5, 0), unidinv(0.5, -1), unidinv(0.5, 6.5));
fprintf('  NaN: p=NaN → %g, N=NaN → %g (NaN)\n', unidinv(NaN, 6), unidinv(0.5, NaN));
