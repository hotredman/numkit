clear
import compat.*

fprintf('=== pascal(5) ===\n');
disp(pascal(5));

fprintf('=== hilb(4) ===\n');
disp(hilb(4));

fprintf('=== invhilb(4) (exact integer entries) ===\n');
disp(invhilb(4));

fprintf('=== wilkinson(7) (eigenvalue test, diag = [3 2 1 0 1 2 3]) ===\n');
disp(wilkinson(7));

fprintf('=== hadamard(4) ===\n');
disp(hadamard(4));
H = hadamard(8);
fprintf('  hadamard(8): H*H'' == 8*I -> %d (expect 1)\n', max(max(abs(H*H' - 8*eye(8)))) == 0);

fprintf('=== rosser() (top 4 rows) ===\n');
R = rosser();
disp(R(1:4,:));
fprintf('  symmetric: %d (expect 1)\n', max(max(abs(R - R'))) == 0);

% Sanity: H*invhilb(H) ~= I
H4 = hilb(4); IH4 = invhilb(4);
fprintf('\nmax(abs(hilb(4)*invhilb(4) - I)) = %g (expect <1e-10)\n', ...
        max(max(abs(H4*IH4 - eye(4)))));
