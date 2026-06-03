clear
import compat.*

% istrellis — validate a convolutional-code trellis structure.

t = poly2trellis(3, [6 7]);
fprintf('istrellis(poly2trellis(3,[6 7])) = %d  (expect 1)\n', istrellis(t));
fprintf('istrellis(struct(''a'',1))        = %d  (expect 0)\n', istrellis(struct('a', 1)));
fprintf('istrellis(5)                     = %d  (expect 0)\n', istrellis(5));
fprintf('istrellis([1 2 3])               = %d  (expect 0)\n', istrellis([1 2 3]));
