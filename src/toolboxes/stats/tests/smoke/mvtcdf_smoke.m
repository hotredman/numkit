clear

fprintf('mvtcdf(0.5, 1, 5)                          = %.10f  (MATLAB: 0.6808505642)\n', mvtcdf(0.5, 1, 5));
fprintf('mvtcdf([0.5 0.3], [1 0.5; 0.5 1], 5)       = %.10f  (MATLAB: 0.4909888137)\n', mvtcdf([0.5 0.3], [1 0.5; 0.5 1], 5));
fprintf('mvtcdf([0.5 0.3 0.7], eye(3), 5)           = %.10f  (MATLAB: 0.3144752061)\n', mvtcdf([0.5 0.3 0.7], eye(3), 5));

% Multi-row input
P = mvtcdf([0 0; 0.5 0.5; 1 1; -0.5 -0.5], [1 0.5; 0.5 1], 5);
fprintf('\nMulti-row P column:\n');
disp(P);
fprintf('(should be ~ [0.25; 0.55; 0.78; 0.10])\n');
