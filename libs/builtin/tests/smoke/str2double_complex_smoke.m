clear

import compat.*

% bugs/builtin/str2double-complex.md — str2double parses complex-number strings
% (was NaN). Real strings stay real double (zero regression).

forms = {'1+2i','1-2i','2i','-3i','i','-i','1+2j','3.5+1.5i','1e-3+2i','1e3i','-2-3i','.5i','1+i',' 2 + 3i ','Infi'};
for k = 1:numel(forms)
    r = str2double(forms{k});
    fprintf('str2double(''%s'') = %.6g %+.6gi\n', forms{k}, real(r), imag(r));
end

fprintf('--- reals unchanged ---\n');
fprintf('str2double(''5'')=%g isreal=%d (expect 5 / 1)\n', str2double('5'), isreal(str2double('5')));
fprintf('str2double(''1.5'')=%g, str2double(''1+2I'')=%g (capital I -> NaN)\n', str2double('1.5'), str2double('1+2I'));

c = str2double({'1+2i','3','4-1i'});
fprintf('cell {''1+2i'',''3'',''4-1i''}: real=[%g %g %g] imag=[%g %g %g] isreal=%d (expect 0)\n', ...
        real(c(1)), real(c(2)), real(c(3)), imag(c(1)), imag(c(2)), imag(c(3)), isreal(c));
