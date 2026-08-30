clear

fprintf('=== expstat ===\n');

[m, v] = expstat(2);
fprintf('  mu=2 : m=%g v=%g (expect 2 / 4)\n', m, v);

[m, v] = expstat([1 2 5 10]);
fprintf('  vector m = [%g %g %g %g] (expect [1 2 5 10])\n', m(1), m(2), m(3), m(4));
fprintf('  vector v = [%g %g %g %g] (expect [1 4 25 100])\n', v(1), v(2), v(3), v(4));

fprintf('\n--- edges ---\n');
fprintf('  mu=0  : %g (expect NaN)\n', expstat(0));
fprintf('  mu<0  : %g (expect NaN)\n', expstat(-1));
