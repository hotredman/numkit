clear
import compat.*

fprintf('=== cholupdate ===\n');

A = [4 2 1; 2 5 3; 1 3 6];
R = chol(A);
fprintf('A is 3x3 SPD; R = chol(A) upper.\n');

% Update '+': R1'*R1 = A + x*x'.
x = [1; 2; 3];
R1 = cholupdate(R, x);
fprintf('  update   ||R1''R1 - (A + x*x'')||_F = %.2e\n', norm(R1'*R1 - (A + x*x'), 'fro'));

% Downdate '-': R2'*R2 = A - y*y'  (if still PD).
y = [0.1; 0.1; 0.1];
R2 = cholupdate(R, y, '-');
fprintf('  downdate ||R2''R2 - (A - y*y'')||_F = %.2e\n', norm(R2'*R2 - (A - y*y'), 'fro'));

% '+' is the default — confirm equivalence with explicit '+'.
RD = cholupdate(R, x);
RP = cholupdate(R, x, '+');
fprintf('  default sign == "+": max|RD - RP| = %.2e\n', max(max(abs(RD - RP))));

% Bad downdate throws (would break positive-definiteness).
fprintf('\nbad downdate (||y|| too large) should throw:\n');
try
    cholupdate(R, [10; 10; 10], '-');
    fprintf('  ERROR: did not throw\n');
catch ME
    fprintf('  caught: %s\n', ME.message);
end

% Scalar special case.
fprintf('\nscalar: chol(9) → 3; cholupdate(3, 4) → sqrt(25) = 5\n');
fprintf('  R1 = %g\n', cholupdate(chol(9), 4));
