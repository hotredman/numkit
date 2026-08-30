clear

fprintf('=== raylpdf ===\n');
fprintf('  Rayl(b=1) at 2 : %.6f (expect 0.270671)\n', raylpdf(2, 1));
y = raylpdf([0 1 2 5], 1);
fprintf('  vec x          : [%g %.4f %.4f %g]\n', y(1), y(2), y(3), y(4));
fprintf('  edges          : x<0 → %g (0), b=0 → %g, b<0 → %g (NaN)\n', raylpdf(-1,1), raylpdf(2,0), raylpdf(2,-1));
