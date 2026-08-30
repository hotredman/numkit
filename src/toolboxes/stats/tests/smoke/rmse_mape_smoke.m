clear

F = [1 2; 3 4];
A = [1.1 1.9; 2.8 4.2];

fprintf('=== rmse ===\n');
disp(rmse(F, A));
fprintf('  rmse all     = %.4f\n', rmse(F, A, 'all'));
fprintf('  rmse [1 2]   = %.4f\n', rmse(F, A, [1 2]));

fprintf('\n=== mape ===\n');
fprintf('  mape all     = %.4f\n', mape(F, A, 'all'));
