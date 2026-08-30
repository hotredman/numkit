clear

% step / impulse / lsim third output x = state trajectory (time x states).
% Use an explicit ss() system so the realization (hence x) is fixed.
sys = ss([-2 -1; 1 0], [1; 0], [0 1], 0);
t = (0:0.5:2)';

[y, tt, x] = step(sys, t);
fprintf('step y    : %s\n', mat2str(y(:).', 8));      % [0 .0902 .2642 .4422 .5940]
fprintf('step x sz : %dx%d\n', size(x,1), size(x,2)); % 5x2
fprintf('x col1    : %s\n', mat2str(x(:,1).', 8));    % [0 .3033 .3679 .3347 .2707]
fprintf('y==x(:,2) : %d\n', isequal(y(:), x(:,2)));   % 1  (C = [0 1])

% lsim with a unit input reproduces the step response.
[yl, tl, xl] = lsim(sys, ones(size(t)), t);
fprintf('lsim==step: %d\n', max(abs(yl(:) - y(:))) < 1e-12);  % 1

% impulse trajectory shape.
[yi, ti, xi] = impulse(sys, t);
fprintf('imp x sz  : %dx%d\n', size(xi,1), size(xi,2));        % 5x2
