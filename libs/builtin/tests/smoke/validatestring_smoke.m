clear

import compat.*

% validatestring: validate a string against a set of allowed values.
% Implemented 2026-05-30 (was an undefined function). vs MATLAB R2025b.

fprintf('=== exact / prefix / case-insensitive ===\n');
fprintf('orange -> [%s] (expect orange)\n', validatestring('orange', {'apple','orange'}));
fprintf('app    -> [%s] (expect apple)\n',  validatestring('app', {'apple','orange'}));
fprintf('APP    -> [%s] (expect apple)\n',  validatestring('APP', {'apple','orange'}));

fprintf('\n=== exact match wins over prefix ===\n');
fprintf('in in {in,input} -> [%s] (expect in)\n', validatestring('in', {'in','input'}));

fprintf('\n=== shortest-prefix-of-all tie-break ===\n');
fprintf('appl in {apple,applesauce} -> [%s] (expect apple)\n', ...
        validatestring('appl', {'apple','applesauce'}));

fprintf('\n=== string array input ===\n');
fprintf('app in ["apple" "orange"] -> [%s] (expect apple)\n', ...
        validatestring("app", ["apple" "orange"]));

fprintf('\n=== ambiguous and no-match throw ===\n');
try
    validatestring('a', {'apple','apricot'});
    fprintf('ambiguous: NO ERROR (unexpected)\n');
catch
    fprintf('ambiguous: threw (expected)\n');
end
try
    validatestring('xyz', {'apple','orange'});
    fprintf('no-match: NO ERROR (unexpected)\n');
catch
    fprintf('no-match: threw (expected)\n');
end
