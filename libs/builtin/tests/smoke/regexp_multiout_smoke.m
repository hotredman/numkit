clear

import compat.*

% regexp default positional multi-output [start, end, tokenExtents, match,
% tokens, names, split]. Bug fixed 2026-05-30: regexp returned only the
% start indices, so [s,e]=regexp(...) errored. vs MATLAB R2025b.

fprintf('=== [start, end] ===\n');
[s, e] = regexp('a1b2', '\d');
fprintf('start = %s  end = %s (expect [2 4] / [2 4])\n', mat2str(s), mat2str(e));

fprintf('\n=== full positional [s,e,te,m,t,nm,sp] ===\n');
[s2, e2, te, m, t, nm, sp] = regexp('a1b2', '(\d)');
fprintf('tokenExtents{1} = %s (expect [2 2])\n', mat2str(te{1}));
fprintf('match{1} = %s  tokens{1}{1} = %s\n', m{1}, t{1}{1});
fprintf('split = {%s, %s} (expect {a, b})\n', sp{1}, sp{2});

fprintf('\n=== single output (start indices) unchanged ===\n');
fprintf('regexp(...) = %s (expect [2 4])\n', mat2str(regexp('a1b2', '\d')));

fprintf('\n=== option string still selects a single output ===\n');
mm = regexp('a1b2c3', '\d', 'match');
fprintf('match opt count = %d first = %s (expect 3 / 1)\n', numel(mm), mm{1});
