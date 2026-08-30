clear

n = 2000;
u = ((1:n)' - 0.5) / n;

x = gevinv(u, 0.2, 1.0, 0.5);
[p, pci] = gevfit(x);
fprintf('Frechet GEV(0.2, 1.0, 0.5):\n');
fprintf('  parm = [%.6f %.6f %.6f]  (MATLAB: [0.200069 1.000000 0.500230])\n', p(1), p(2), p(3));
fprintf('  CI lo: [%.4f %.4f %.4f]  (MATLAB: [0.1647 0.9614 0.4507])\n', pci(1,1), pci(1,2), pci(1,3));
fprintf('  CI hi: [%.4f %.4f %.4f]  (MATLAB: [0.2354 1.0402 0.5498])\n', pci(2,1), pci(2,2), pci(2,3));

x2 = gevinv(u, -0.2, 2.0, 1.0);
p2 = gevfit(x2);
fprintf('\nReverse-Weibull GEV(-0.2, 2.0, 1.0):\n');
fprintf('  parm = [%.6f %.6f %.6f]  (MATLAB: [-0.200875 1.999846 1.001063])\n', p2(1), p2(2), p2(3));

x3 = gevinv(u, 0, 1.5, 0.0);
p3 = gevfit(x3);
fprintf('\nGumbel-max limit GEV(0, 1.5, 0):\n');
fprintf('  parm = [%.6f %.6f %.6f]  (k expected ~0)\n', p3(1), p3(2), p3(3));
