clear

import compat.*

% bugs/builtin/sort-char.md — sort on char input.
% MATLAB sorts char by CODE POINT, PRESERVING the char class on the values;
% the 2nd-output index stays double.

[S, I] = sort('dcba');
fprintf('sort(''dcba'') = %s   expect abcd, ischar=%d (expect 1)\n', S, ischar(S));
fprintf('  index = [%g %g %g %g]   expect [4 3 2 1], islogical(I)=%d (expect 0)\n', ...
        I(1), I(2), I(3), I(4), islogical(I));

dd = sort('dcba', 'descend');
fprintf('sort(''dcba'',''descend'') = %s   expect dcba\n', dd);

[Sr, Ir] = sort('cbacb');
fprintf('sort(''cbacb'') = %s   expect abbcc (stable)\n', Sr);
fprintf('  index = [%g %g %g %g %g]   expect [3 2 5 1 4]\n', Ir(1), Ir(2), Ir(3), Ir(4), Ir(5));

C = sort(['bd'; 'ca']);
fprintf('sort([''bd'';''ca'']) col = [%s; %s]   expect [ba; cd]\n', C(1,:), C(2,:));

R = sort(['bd'; 'ca'], 2);
fprintf('sort(...,2) row = [%s; %s]   expect [bd; ac]\n', R(1,:), R(2,:));

fprintf('sort(''x'') = %s   expect x\n', sort('x'));
e = sort('');
fprintf('sort('''') numel = %d   expect 0, ischar=%d (expect 1)\n', numel(e), ischar(e));
