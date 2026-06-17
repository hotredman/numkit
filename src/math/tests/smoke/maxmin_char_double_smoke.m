clear

import compat.*

% bugs/builtin/maxmin-char-double.md — max/min of a char array return DOUBLE
% (the code point), NOT char. (mode KEEPS the char class — that is correct.)

m = max('abc');
fprintf('max(''abc'') = %g  ischar=%d  isa double=%d   expect 99 / 0 / 1\n', m, ischar(m), isa(m,'double'));

mn = min('abc');
fprintf('min(''abc'') = %g  ischar=%d   expect 97 / 0\n', mn, ischar(mn));

[v, i] = max('abc');
fprintf('[v,i]=max(''abc'') = %g / %g   expect 99 / 3\n', v, i);

cm = max(['abc'; 'xyz']);
fprintf('max([''abc'';''xyz'']) col = [%g %g %g]   expect [120 121 122]\n', cm(1), cm(2), cm(3));

fprintf('max(''abc'',[],''all'') = %g   expect 99\n', max('abc',[],'all'));
% NOTE: binary max('a','c') is a MATLAB ERROR ("Invalid second argument");
% numkit is lenient but it is not a MATLAB-matched form, so not shown here.

mo = mode('abc');
fprintf('mode(''abc'') = %g  ischar=%d   expect 97 / 1  (mode KEEPS char)\n', double(mo), ischar(mo));
