clear

fprintf('ncfcdf(1.5, 5, 10, 3)         = %.12f  (MATLAB: 0.491323141971)\n', ncfcdf(1.5, 5, 10, 3));
fprintf('ncfcdf(1.5, 5, 10, 3, upper)  = %.12f  (MATLAB: 0.508676858029)\n', ncfcdf(1.5, 5, 10, 3, 'upper'));

y = ncfcdf([0.5 1.0 2.0 3.0], 5, 10, 3);
fprintf('ncfcdf vector: '); disp(y);
fprintf('       expect: [0.0923 0.2973 0.6391 0.8167]\n');

% Central limit
fprintf('\nDelta=0 limit: ncfcdf(1.5, 5, 10, 0) = %.12f\n', ncfcdf(1.5, 5, 10, 0));
fprintf('               fcdf  (1.5, 5, 10  ) = %.12f  (should match)\n', fcdf(1.5, 5, 10));

x = ncfinv(0.3, 5, 10, 3);
fprintf('\nncfinv(0.3, 5, 10, 3) = %.10f  (MATLAB: 1.0063340603)\n', x);
fprintf('  round-trip ncfcdf = %.10f  (expect 0.3)\n', ncfcdf(x, 5, 10, 3));

x = ncfinv([0.1 0.5 0.9], 5, 10, 3);
fprintf('\nncfinv vector: '); disp(x);
fprintf('       expect: [0.5216 1.5254 3.9619]\n');
