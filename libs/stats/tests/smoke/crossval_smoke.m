clear
import compat.*

fprintf('=== crossval (k-fold cross-validation) ===\n');

X = (1:20)';
Y = 2 * X + 1;
predfun = @(Xtr, Ytr, Xte, Yte) mean((Xte * mldivide(Xtr, Ytr) - Yte).^2);

mse10 = crossval(predfun, X, Y);
fprintf('  10-fold MSE shape: [%d %d]\n', size(mse10,1), size(mse10,2));
fprintf('  per-fold MSE: '); disp(mse10');
fprintf('  mean MSE = %g\n', mean(mse10));

mse5 = crossval(predfun, X, Y, 'kfold', 5);
fprintf('\n  5-fold MSE shape: [%d %d]\n', size(mse5,1), size(mse5,2));

% Perfect identity model
Xid = (1:20)'; Yid = Xid;
mse_perfect = crossval(predfun, Xid, Yid);
fprintf('\n  Identity y=x: mean MSE = %g (expect ~0)\n', mean(mse_perfect));
