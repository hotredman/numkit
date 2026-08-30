clear

% mle 2nd output pci (parameter confidence intervals). Fixed 2026-06-05
% (bugs/stats/mle-output.md). Reference: MATLAB R2025b.

[ph, pci] = mle([2 3 4 5 6 4 3]);
fprintf('normal phat = [%.6f %.6f]\n', ph(1), ph(2));
fprintf('  pci (2x2) = [%.6f %.6f; %.6f %.6f]\n', pci(1,1), pci(1,2), pci(2,1), pci(2,2));
fprintf('  expect      [2.613054 0.866829; 5.101232 2.962187]\n');

[pe, pcie] = mle([1.2 0.5 2.1 0.8 3.0 1.5], 'distribution', 'exp');
fprintf('exp   phat = %.6f  pci = [%.6f %.6f]  (expect 1.516667 [0.779889 4.132805])\n', pe, pcie(1), pcie(2));

[pp, pcip] = mle([2 3 1 4 2 5 3], 'distribution', 'poisson');
fprintf('poiss phat = %.6f  pci = [%.6f %.6f]  (expect 2.857143 [1.745217 4.412625])\n', pp, pcip(1), pcip(2));

[~, pcil] = mle([1.2 2.5 0.8 3.1 1.9 4.2 2.0], 'distribution', 'lognormal');
fprintf('logn  pci = [%.6f %.6f; %.6f %.6f]  (expect [0.162710 0.362113; 1.202134 1.237439])\n', ...
        pcil(1,1), pcil(1,2), pcil(2,1), pcil(2,2));

[~, pa] = mle([2 3 4 5 6 4 3], 'Alpha', 0.01);
fprintf('alpha=0.01: pci(1,1) = %.6f  (expect 1.972167, wider)\n', pa(1,1));
