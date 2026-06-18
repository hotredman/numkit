clear
import compat.*

% wenergy: percent of energy in the approximation (Ea) + each detail (Ed).
% bugs/wavelet/wenergy. Ed is finest-first (level 1 ... level N).

[c, l] = wavedec([1 2 3 4 5 6 7 8], 2, 'db1');
[Ea, Ed] = wenergy(c, l);
fprintf('1:8 db1: Ea=%.10f Ed=[%s] sum=%.4f\n', Ea, num2str(Ed, '%.8f '), Ea + sum(Ed));
fprintf('  (expect Ea=95.0980392, Ed=[0.98039216 3.92156863], sum=100)\n');

[c, l] = wavedec(1:16, 3, 'db1');
[Ea, Ed] = wenergy(c, l);
fprintf('1:16 db1: Ea=%.8f Ed=[%s]\n', Ea, num2str(Ed, '%.6f '));
fprintf('  (expect Ea=94.38502674, Ed=[0.267380 1.069519 4.278075] -- finest level first)\n');

[c, l] = wavedec(sin(1:32), 2, 'db2');
[Ea, Ed] = wenergy(c, l);
fprintf('sin db2: Ea=%.6f Ed=[%s]  (expect Ea=34.187599, Ed=[9.833376 55.979025])\n', ...
        Ea, num2str(Ed, '%.6f '));
