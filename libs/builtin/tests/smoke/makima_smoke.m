clear
import compat.*

fprintf('=== makima — modified Akima interpolation ===\n');

x = 1:5;
y = [1 4 9 16 25];

fprintf('\nx = %s\n', mat2str(x));
fprintf('y = x.^2 = %s\n', mat2str(y));

% Hermite property — passes through data exactly.
fprintf('\nmakima at data points (should equal y):\n');
disp(makima(x, y, x));

% Mid-interval query.
fprintf('makima at [1.5 2.5 3.5 4.5]:\n');
disp(makima(x, y, [1.5 2.5 3.5 4.5]));
fprintf('   reference (x.^2 at same xq) = %s\n', ...
        mat2str([2.25 6.25 12.25 20.25]));

% Flat data — proves the modified weight avoids zero-weight blow-up
% that the original Akima 1970 formula had.
fprintf('\nmakima of constant y=[3 3 3 3 3] at 2.5:\n');
fprintf('   %g  (expect 3 — modified Akima handles flat data)\n', ...
        makima(1:5, [3 3 3 3 3], 2.5));

% Vector output preserving xq orientation (column input → column output).
fprintf('\nmakima with column xq:\n');
disp(makima(x, y, [1.5; 2.5; 3.5]));
