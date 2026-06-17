clear

import compat.*

% strsplit second output (matched delimiters). Bug fixed 2026-05-30:
% strsplit returned only the tokens, so [t,m]=strsplit(...) errored.
% vs MATLAB R2025b.

fprintf('=== matched delimiters ===\n');
[t, m] = strsplit('a,b;c', {',', ';'});
fprintf('tokens  = {%s} (expect a, b, c)\n', strjoin(t, ', '));
fprintf('matches = {%s} (expect , and ;)\n', strjoin(m, ' | '));
fprintf('numel(m) = %d (expect 2)\n', numel(m));

fprintf('\n=== a collapsed run is one match ===\n');
[t2, m2] = strsplit('a,,b', ',');
fprintf('matches = {%s} numel = %d (expect ,, and 1)\n', m2{1}, numel(m2));

fprintf('\n=== default whitespace ===\n');
[t3, m3] = strsplit('hi  there');
fprintf('match = [%s] numel = %d (expect "  " collapsed, 1)\n', m3{1}, numel(m3));

fprintf('\n=== single output unchanged ===\n');
fprintf('numel = %d (expect 3)\n', numel(strsplit('a,b,c', ',')));
