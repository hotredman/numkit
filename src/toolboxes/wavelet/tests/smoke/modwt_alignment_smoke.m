clear
import compat.*

% modwt per-coefficient alignment now matches MATLAB R2025b.
% MODWT filters = wrev(Lo_D)/sqrt2, wrev(Hi_D)/sqrt2, look-back circular.
x = [1 2 3 4 5 6 7 8];

w = modwt(x, 'haar', 2);
fprintf('haar W1(1:4) = %s\n', mat2str(w(1,1:4), 8));   % [-3.5 0.5 0.5 0.5]
fprintf('haar V2(1:4) = %s\n', mat2str(w(3,1:4), 8));   % [5.5 4.5 3.5 2.5]

wd = modwt(x, 'db2', 1);
fprintf('db2  W1(1:3) = %s\n', mat2str(wd(1,1:3), 10)); % [0.7320508076 2 -2.732050808]

% Inverse still exact.
r = imodwt(w, 'haar');
fprintf('imodwt err   = %.2e\n', max(abs(r(:).' - x)));  % ~0
