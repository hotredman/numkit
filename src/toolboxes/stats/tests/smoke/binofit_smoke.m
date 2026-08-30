clear

fprintf('=== binofit ===\n');

[ph, pci] = binofit(7, 10);
fprintf('  scalar (7/10): phat=%.4f, pci=[%.6f %.6f]\n', ph, pci(1), pci(2));
fprintf('    expect:               phat=0.7000, pci=[0.347547 0.933260]\n\n');

[ph, pci] = binofit([3 5 7]', [10 10 10]');
fprintf('  vector phat   = [%.2f %.2f %.2f] (expect [0.30 0.50 0.70])\n', ph(1), ph(2), ph(3));
fprintf('  vector pci(1) = [%.6f %.6f] (expect [0.066740 0.652453])\n', pci(1,1), pci(1,2));
fprintf('  vector pci(3) = [%.6f %.6f] (expect [0.347547 0.933260])\n\n', pci(3,1), pci(3,2));

[ph, pci] = binofit(0, 10);
fprintf('  edge x=0:  phat=%.4f, pci=[%.4f %.6f] (expect 0.0, [0.0 0.308497])\n', ph, pci(1), pci(2));

[ph, pci] = binofit(10, 10);
fprintf('  edge x=n:  phat=%.4f, pci=[%.6f %.4f] (expect 1.0, [0.691503 1.0])\n\n', ph, pci(1), pci(2));

[ph, pci] = binofit(7, 10, 0.01);
fprintf('  alpha=0.01: phat=%.4f, pci=[%.6f %.6f]\n', ph, pci(1), pci(2));
fprintf('    expect:                 phat=0.7000, pci wider than alpha=0.05 case\n');
