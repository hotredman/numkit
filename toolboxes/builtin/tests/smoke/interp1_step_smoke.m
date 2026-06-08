clear
import compat.*
% interp1 'previous'/'next' step interpolation (were unimplemented). MATLAB R2025b.
fprintf('next 1.5     = %g (expect 20)\n',  interp1([1 2 3],[10 20 30],1.5,'next'));
fprintf('previous 1.5 = %g (expect 10)\n',  interp1([1 2 3],[10 20 30],1.5,'previous'));
fprintf('next @2      = %g (expect 20)\n',  interp1([1 2 3],[10 20 30],2,'next'));
fprintf('previous @2  = %g (expect 20)\n',  interp1([1 2 3],[10 20 30],2,'previous'));
fprintf('previous 3.5 = %g (expect NaN)\n', interp1([1 2 3],[10 20 30],3.5,'previous'));
fprintf('next 0.5     = %g (expect NaN)\n', interp1([1 2 3],[10 20 30],0.5,'next'));
v = interp1([1 2 3],[10 20 30],[1.5 2.5],'previous');
fprintf('previous vec = [%g %g] (expect 10 20)\n', v(1), v(2));
