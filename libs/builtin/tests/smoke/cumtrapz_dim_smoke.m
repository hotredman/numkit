clear
import compat.*

% cumtrapz(Y,dim) and cumtrapz(X,Y,dim): cumulative trapezoid along a dim.

% Vector default.
v = cumtrapz([1 2 3 4]);
fprintf('vec        : %s\n', mat2str(v));            % expect [0 1.5 4 7.5]

A = [1 2; 3 4];

% dim 1 (== default): down columns.
c1 = cumtrapz(A, 1);
fprintf('dim1 col1  : %s\n', mat2str(c1(:,1).'));    % expect [0 2]
fprintf('dim1 col2  : %s\n', mat2str(c1(:,2).'));    % expect [0 3]

% dim 2: along rows.
c2 = cumtrapz(A, 2);
fprintf('dim2 row1  : %s\n', mat2str(c2(1,:)));      % expect [0 1.5]
fprintf('dim2 row2  : %s\n', mat2str(c2(2,:)));      % expect [0 3.5]

% X,Y two-vector form.
xy = cumtrapz([0 1 2], [3 4 5]);
fprintf('X,Y vec    : %s\n', mat2str(xy));           % expect [0 3.5 8]

% X,Y,dim row-wise: X is a coordinate vector of length size(Y,2).
X = [0 1 2];
Y = [3 4 5; 1 1 1];
c3 = cumtrapz(X, Y, 2);
fprintf('XYdim2 r1  : %s\n', mat2str(c3(1,:)));      % expect [0 3.5 8]
fprintf('XYdim2 r2  : %s\n', mat2str(c3(2,:)));      % expect [0 1 2]

% Vector along singleton dim is a no-op (matches MATLAB).
fprintf('vec dim1   : %s\n', mat2str(cumtrapz([1 2 3 4], 1)));  % expect [0 0 0 0]
fprintf('vec dim2   : %s\n', mat2str(cumtrapz([1 2 3 4], 2)));  % expect [0 1.5 4 7.5]
