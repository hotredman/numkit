clear

import compat.*

% lower / upper / strtrim / deblank / strip applied to a CELL array —
% DEEP-PROBE 2026-05-31. All five threw "Not a char array" on a cell input.
% MATLAB applies them element-wise, returning a cell of char vectors with the
% same shape; a char/string scalar is unchanged. Reference: MATLAB R2025b.

fprintf('=== lower / upper ===\n');
lo = lower({'AbC','XyZ'});
up = upper({'aBc','xYz'});
fprintf('lower -> {%s, %s}  (expect abc, xyz)\n', lo{1}, lo{2});
fprintf('upper -> {%s, %s}  (expect ABC, XYZ)\n', up{1}, up{2});

fprintf('\n=== strtrim (interior kept) / deblank (trailing only) ===\n');
st = strtrim({'  a b ','  x  '});
fprintf('strtrim -> [%s][%s]  (expect [a b][x])\n', st{1}, st{2});
db = deblank({'a  ','  b '});
fprintf('deblank -> [%s][%s]  (expect [a][  b])\n', db{1}, db{2});

fprintf('\n=== strip (both) / strip with char ===\n');
sp = strip({'  a  ','  b'});
fprintf('strip -> [%s][%s]  (expect [a][b])\n', sp{1}, sp{2});
sx = strip({'xxaxx','xb'}, 'both', 'x');
fprintf('strip x -> [%s][%s]  (expect [a][b])\n', sx{1}, sx{2});

fprintf('\n=== shape preserved + scalar path unchanged ===\n');
col = lower({'AB';'CD'});
fprintf('column cell -> size %dx%d  (expect 2x1)\n', size(col,1), size(col,2));
fprintf('scalar lower(HELLO) = %s (class %s)\n', lower('HELLO'), class(lower('HELLO')));
