clear

fprintf('=== chebwin (Dolph-Chebyshev) ===\n');
fprintf('  Bug fix 2026-05-08: previous FFT-based impl returned\n');
fprintf('  all-ones for even N and a wrongly-shifted shape for odd N.\n\n');

fprintf('  chebwin(8, 100): '); fprintf('%.4f ', chebwin(8, 100)); fprintf('\n');
fprintf('  expect:           0.0364 0.2254 0.6242 1.0000 1.0000 0.6242 0.2254 0.0364\n');

fprintf('  chebwin(8, 60):  '); fprintf('%.4f ', chebwin(8, 60)); fprintf('\n');
fprintf('  expect:           0.0685 0.3032 0.6868 1.0000 1.0000 0.6868 0.3032 0.0685\n');

fprintf('  chebwin(7, 100): '); fprintf('%.4f ', chebwin(7, 100)); fprintf('\n');
fprintf('  expect:           0.0565 0.3166 0.7601 1.0000 0.7601 0.3166 0.0565\n');

fprintf('  chebwin(8, 30):  '); fprintf('%.4f ', chebwin(8, 30)); fprintf('\n');
fprintf('  expect:           0.2622 0.5187 0.8120 1.0000 1.0000 0.8120 0.5187 0.2622\n');

fprintf('  chebwin(1, 100): %g (expect 1)\n', chebwin(1, 100));
