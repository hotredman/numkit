clear

fprintf('=== seqperiod ===\n');
[p, n] = seqperiod([1 2 3 1 2 3 1 2 3]);
fprintf('  [1 2 3 1 2 3 1 2 3]: p=%d nr=%g (expect 3, 3)\n', p, n);
[p, n] = seqperiod([1 2 3 4]);
fprintf('  [1 2 3 4]: p=%d nr=%g (expect 4, 1)\n', p, n);
[p, n] = seqperiod([1 1 1 1]);
fprintf('  [1 1 1 1]: p=%d nr=%g (expect 1, 4)\n', p, n);

fprintf('\n=== zerocrossrate ===\n');
fprintf('  [1 -1 1 -1]: %g (expect 0.875)\n', zerocrossrate([1 -1 1 -1]));
fprintf('  [1 1]: %g (expect 0.25)\n', zerocrossrate([1 1]));
fprintf('  [1 -1 2 -2 3 -3 4 -4]: %g (expect 0.9375)\n', zerocrossrate([1 -1 2 -2 3 -3 4 -4]));
[r, c] = zerocrossrate([1 -1 1 -1]);
fprintf('  with count: rate=%g count=%g\n', r, c);

fprintf('\n=== cusum ===\n');
% Sequence with mean shift at index 21
x = [zeros(20,1); 3*ones(20,1)];
[iup, ilo] = cusum(x, 5, 1, 0, 1);
fprintf('  shift sequence cusum(x, 5, 1, 0, 1): iupper=%d (expect ~22)\n', iup);
[iup, ilo, us, ls] = cusum(x, 5, 1, 0, 1);
fprintf('  uppersum at idx 25 = %g\n', us(25));

fprintf('\nKNOWN GAPs (deferred):\n');
fprintf('  zerocrossrate matrix/N-D + Name=Value args\n');
fprintf('  seqperiod matrix/N-D (column-wise)\n');
fprintf('  cusum no-output plotting form\n');
