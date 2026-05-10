clear
import compat.*

fprintf('=== issparse (always false in numkit -- no sparse storage) ===\n');

fprintf('issparse([1 2; 3 4])     = %d (expect 0)\n', issparse([1 2; 3 4]));
fprintf('issparse(zeros(3,3))     = %d (expect 0)\n', issparse(zeros(3,3)));
fprintf('issparse(eye(3))         = %d (expect 0)\n', issparse(eye(3)));
fprintf('issparse(0)              = %d (expect 0)\n', issparse(0));
fprintf('issparse([])             = %d (expect 0)\n', issparse([]));
fprintf('issparse(true)           = %d (expect 0)\n', issparse(true));
fprintf('issparse(''abc'')          = %d (expect 0)\n', issparse('abc'));
fprintf('issparse({1,2})          = %d (expect 0)\n', issparse({1, 2}));

fprintf('\nKNOWN GAP: numkit has no sparse-matrix storage class.\n');
fprintf('issparse always returns false for any input. This matches\n');
fprintf('MATLAB on dense inputs (which is all numkit can produce).\n');
