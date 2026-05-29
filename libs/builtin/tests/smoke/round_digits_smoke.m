clear
import compat.*
% round(x,N) decimals + round(x,N,'significant'). vs MATLAB R2025b.
fprintf('round(3.14159,2)            = %g (expect 3.14)\n', round(3.14159, 2));
fprintf('round(3.14159,4)            = %g (expect 3.1416)\n', round(3.14159, 4));
fprintf('round(12345,-2)             = %g (expect 12300)\n', round(12345, -2));
fprintf('round(3.14159,3,significant)= %g (expect 3.14)\n', round(3.14159, 3, 'significant'));
fprintf('round(12345,2,significant)  = %g (expect 12000)\n', round(12345, 2, 'significant'));
fprintf('round(0.0012345,2,sig)      = %g (expect 0.0012)\n', round(0.0012345, 2, 'significant'));
