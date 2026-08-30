clear

fprintf('=== poisspdf ===\n');
fprintf('  Pois(2) at 3      : %.6f (expect 0.180447)\n', poisspdf(3, 2));
y = poisspdf([0 1 3 10], 2);
fprintf('  vector k          : [%.4f %.4f %.4f %g]\n', y(1), y(2), y(3), y(4));
fprintf('  out-of-support    : x<0 → %g, x=2.5 → %g (both 0)\n', poisspdf(-1,2), poisspdf(2.5,2));
fprintf('  lam=0 (degenerate): k=0 → %g (1), k=3 → %g (0)\n', poisspdf(0,0), poisspdf(3,0));
fprintf('  lam<0             : %g (NaN)\n', poisspdf(3,-1));
