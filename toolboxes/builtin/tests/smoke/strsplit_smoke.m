clear

import compat.*

% strsplit — cell-array & multi-char delimiters + CollapseDelimiters option.
% Cell/multi/collapse support added 2026-05-30 (DEEP-PROBE). vs MATLAB R2025b.

show = @(c) fprintf('  n=%d: [%s]\n', numel(c), strjoin(cellfun(@(x) ['<' x '>'], c, 'UniformOutput', false), ','));

fprintf('=== cell-array delimiters (longest-match) ===\n');
fprintf('strsplit("a, b; c", {", ", "; "}) expect n=3 [<a><b><c>]:\n');
show(strsplit('a, b; c', {', ', '; '}));

fprintf('=== multi-char delimiter string ===\n');
fprintf('strsplit("a==b==c", "==") expect n=3 [<a><b><c>]:\n');
show(strsplit('a==b==c', '=='));

fprintf('=== CollapseDelimiters=true (default): leading/trailing empties kept ===\n');
fprintf('strsplit(",a,b,", ",") expect n=4 [<><a><b><>]:\n');
show(strsplit(',a,b,', ','));
fprintf('strsplit("a,,b", ",") expect n=2 [<a><b>] (internal collapsed):\n');
show(strsplit('a,,b', ','));

fprintf('=== CollapseDelimiters=false: split at every occurrence ===\n');
fprintf('strsplit("a,,b", ",", "CollapseDelimiters", false) expect n=3 [<a><><b>]:\n');
show(strsplit('a,,b', ',', 'CollapseDelimiters', false));

fprintf('=== default whitespace delimiter (collapse) ===\n');
fprintf('strsplit("  a  b  ") expect n=4 [<><a><b><>]:\n');
show(strsplit('  a  b  '));
fprintf('strsplit("a b c") expect n=3 [<a><b><c>]:\n');
show(strsplit('a b c'));
