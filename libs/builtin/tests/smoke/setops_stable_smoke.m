clear
import compat.*
% setdiff / union / intersect 'stable' setOrder (MATLAB R2025b).
sd = setdiff([3 1 2 5 4], [2 5], 'stable');
fprintf('setdiff stable: %g %g %g (expect 3 1 4)\n', sd(1), sd(2), sd(3));

un = union([3 1], [2 1], 'stable');
fprintf('union stable  : %g %g %g (expect 3 1 2)\n', un(1), un(2), un(3));

ii = intersect([4 2 3 1], [1 2 4], 'stable');
fprintf('intersect stab: %g %g %g (expect 4 2 1)\n', ii(1), ii(2), ii(3));

% default 'sorted' still sorts.
ss = setdiff([3 1 2 5 4], [2 5]);
fprintf('setdiff sorted: %g %g %g (expect 1 3 4)\n', ss(1), ss(2), ss(3));
