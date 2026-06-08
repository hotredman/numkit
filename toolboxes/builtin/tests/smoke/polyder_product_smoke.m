clear

import compat.*

% bugs/builtin/polyder-product.md — polyder(a,b) with ONE output is the
% derivative of the PRODUCT a*b (= polyder(conv(a,b))), NOT the quotient
% numerator. The 2-output [q,d] quotient form and the 1-arg form are unchanged.

a = polyder([1 0], [1 1]);
fprintf('polyder([1 0],[1 1]) = [%g %g]   expect [2 1]  (d/dx[x*(x+1)]=2x+1)\n', a(1), a(2));

b = polyder([1 2], [1 3]);
fprintf('polyder([1 2],[1 3]) = [%g %g]   expect [2 5]\n', b(1), b(2));

c = polyder([1 0 0], [1 1]);
fprintf('polyder([1 0 0],[1 1]) = [%g %g %g]   expect [3 2 0]\n', c(1), c(2), c(3));

[q, d] = polyder([1 0], [1 1]);
fprintf('[q,d]=polyder([1 0],[1 1]): q=[%g] d=[%g %g %g]   expect q=1 d=[1 2 1] (quotient unchanged)\n', ...
        q(1), d(1), d(2), d(3));

s = polyder([1 2 3]);
fprintf('polyder([1 2 3]) = [%g %g]   expect [2 2] (single-arg unchanged)\n', s(1), s(2));
