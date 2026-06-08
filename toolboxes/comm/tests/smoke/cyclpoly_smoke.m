clear
import compat.*

% cyclpoly — generator polynomial(s) for an (n,k) cyclic code.
% Error Correction Codes / block linear codes, GF(2).

% Default: first generator polynomial found (ascending powers).
p = cyclpoly(7, 4);
fprintf('cyclpoly(7,4)      = '); disp(p);   % expect [1 0 1 1]
fprintf('cyclpoly(15,11)    = '); disp(cyclpoly(15, 11));  % expect [1 0 0 1 1]

% min / max term count.
fprintf('cyclpoly(7,4,min)  = '); disp(cyclpoly(7, 4, 'min'));  % [1 0 1 1]
fprintf('cyclpoly(7,4,max)  = '); disp(cyclpoly(7, 4, 'max'));  % [1 1 0 1]

% all generator polynomials, one per row (sorted by term count).
disp('cyclpoly(7,4,all):');
disp(cyclpoly(7, 4, 'all'));  % expect [1 0 1 1; 1 1 0 1]
