clear

% standardizeMissing — replace nonstandard missing-value indicators with NaN.
% Reference: MATLAB R2025b.

fprintf('== double scalar indicator ==\n');
B = standardizeMissing([1 2 -99 4], -99);
fprintf('  [1 2 -99 4], -99 -> [%g %g %g %g] (e 1 2 NaN 4)  class=%s\n', B(1), B(2), B(3), B(4), class(B));

fprintf('\n== double vector indicator ==\n');
B = standardizeMissing([1 -99 -88 4], [-99 -88]);
fprintf('  [-99 -88] -> [%g %g %g %g] (e 1 NaN NaN 4)\n', B(1), B(2), B(3), B(4));

fprintf('\n== integer types pass through ==\n');
B = standardizeMissing(uint8([1 2 3 5]), 3);
fprintf('  uint8([1 2 3 5], 3) -> [%d %d %d %d] (e 1 2 3 5)  class=%s\n', B(1), B(2), B(3), B(4), class(B));
B = standardizeMissing(int16([1 -99 4]), int16(-99));
fprintf('  int16([1 -99 4], -99) -> [%d %d %d] (e 1 -99 4)  class=%s\n', B(1), B(2), B(3), class(B));

fprintf('\n== single preserved ==\n');
B = standardizeMissing(single([1.5 2 -99 4]), single(-99));
fprintf('  -> [%g %g %g %g] (e 1.5 2 NaN 4)  class=%s\n', double(B(1)), double(B(2)), double(B(3)), double(B(4)), class(B));

fprintf('\n== matrix ==\n');
B = standardizeMissing([1 -99; -99 4], -99);
fprintf('  B(1,1)=%g  B(1,2)=%g  B(2,1)=%g  B(2,2)=%g  (e 1 NaN NaN 4)\n', B(1,1), B(1,2), B(2,1), B(2,2));

fprintf('\n== NaN in indicator does NOT match ==\n');
B = standardizeMissing([1 2 NaN 4], NaN);
fprintf('  [1 2 NaN 4], NaN -> [%g %g %g %g] (e 1 2 NaN 4 — unchanged)\n', B(1), B(2), B(3), B(4));

fprintf('\n== mixed: NaN + -99 indicators ==\n');
B = standardizeMissing([1 NaN -99 4], [NaN -99]);
fprintf('  -> [%g %g %g %g] (e 1 NaN NaN 4)\n', B(1), B(2), B(3), B(4));
