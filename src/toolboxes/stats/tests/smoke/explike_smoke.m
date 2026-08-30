clear

x    = [1 2 3 4 5]';
cens = [0 0 0 1 1]';
freq = [2 2 1 1 1]';

fprintf('=== explike ===\n');
[nL, av] = explike(2, x);
fprintf('  basic   : nL=%.6f  av=%.6f  (expect 10.965736 / 0.4)\n', nL, av);
[nL, av] = explike(2, x, cens);
fprintf('  cens    : nL=%.6f  av=%.6f  (expect  9.579442 / 0.333333)\n', nL, av);
[nL, av] = explike(2, x, [], freq);
fprintf('  freq    : nL=%.6f  av=%.6f  (expect 13.852030 / 0.363636)\n', nL, av);
[nL, av] = explike(2, x, cens, freq);
fprintf('  both    : nL=%.6f  av=%.6f  (expect 12.465736 / 0.307692)\n', nL, av);

fprintf('\n--- edges ---\n');
fprintf('  mu=-1  : %g (expect NaN)\n', explike(-1, x));
fprintf('  mu=0   : %g (expect NaN)\n', explike(0, x));
fprintf('  empty  : %g (expect 0)\n', explike(2, []));
