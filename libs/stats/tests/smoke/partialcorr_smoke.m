clear
import compat.*

fprintf('=== partialcorr (control for confounder) ===\n');
x = [1; 2; 3; 4; 5; 6; 7; 8; 9; 10];
y = [1.2; 1.8; 3.5; 3.9; 5.1; 6.0; 7.2; 7.8; 9.1; 10.0];
z = [1; 4; 2; 5; 3; 6; 8; 7; 9; 10];

fprintf('  naive corr(x, y) (column 1 of corr([x y])):\n');
cn = corr([x y]);
fprintf('    %g (high since both correlate with z)\n', cn(1, 2));
fprintf('  partialcorr(x, y, z) = %g (residual after regressing out z)\n', ...
        partialcorr(x, y, z));

% Multi-column X and Y
X2 = [x x.^2];
Y2 = [y y.^2 y.^3];
P = partialcorr(X2, Y2, z);
fprintf('\n  partialcorr(2-col X, 3-col Y, z) -> 2x3 matrix:\n');
disp(P);
