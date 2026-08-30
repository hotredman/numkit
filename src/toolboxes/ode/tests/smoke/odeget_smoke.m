clear;

% odeget — retrieve a named option from a struct.

o = odeset('RelTol', 1e-9, 'AbsTol', 1e-12, 'MaxStep', 0.05);

fprintf('--- 1) odeget(opts, name) ---\n');
fprintf('RelTol  = %.0e\n', odeget(o, 'RelTol'));
fprintf('AbsTol  = %.0e\n', odeget(o, 'AbsTol'));
fprintf('MaxStep = %.4f\n', odeget(o, 'MaxStep'));

fprintf('\n--- 2) case-insensitive lookup ---\n');
fprintf('reltol  = %.0e\n', odeget(o, 'reltol'));
fprintf('ABSTOL  = %.0e\n', odeget(o, 'ABSTOL'));
fprintf('MaXsTeP = %.4f\n', odeget(o, 'MaXsTeP'));

fprintf('\n--- 3) odeget with default (option empty) ---\n');
fprintf('Jacobian (default 42) = %d\n', odeget(o, 'Jacobian', 42));
fprintf('Events (default ''none'') = %s\n', odeget(o, 'Events', 'none'));
