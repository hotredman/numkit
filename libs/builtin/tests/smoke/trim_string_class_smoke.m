clear
import compat.*

% deblank / strtrim / strip are class-preserving (MATLAB R2025b):
%   string array -> string array (same shape)
%   string scalar -> string scalar
%   char         -> char
%   cell         -> cell of char vectors

% ── string array -> string array ────────────────────────────────
d = deblank(["ab  " "cd "]);
fprintf('deblank(["ab  " "cd "])  isstring=%d numel=%d  v1=%s v2=%s (expect 1, 2, ab, cd)\n', ...
        isstring(d), numel(d), d(1), d(2));
t = strtrim(["  ab " "  cd"]);
fprintf('strtrim(["  ab " "  cd"]) isstring=%d v1=%s v2=%s (expect 1, ab, cd)\n', ...
        isstring(t), t(1), t(2));
p = strip(["xxab" "xcd"], 'left', 'x');
fprintf('strip(["xxab" "xcd"],''left'',''x'') isstring=%d numel=%d v2=%s (expect 1, 2, cd)\n', ...
        isstring(p), numel(p), p(2));

% ── column string array: shape preserved ────────────────────────
cs = strtrim(["  a"; "b  "]);
fprintf('strtrim col str-arr  sz=%dx%d (expect 2x1)\n', size(cs,1), size(cs,2));

% ── string scalar -> string scalar (NOT char) ───────────────────
d1 = deblank("hello   ");
fprintf('deblank("hello   ")  isstring=%d v=%s (expect 1, hello)\n', isstring(d1), d1);

% ── char input -> char (unchanged) ──────────────────────────────
dc = deblank('world  ');
fprintf('deblank(''world  '')  ischar=%d v=%s (expect 1, world)\n', ischar(dc), dc);

% ── cell input -> cell of char vectors (unchanged) ──────────────
ce = strtrim({'  a ', ' b  '});
fprintf('strtrim cell  iscell=%d v1=%s v2=%s (expect 1, a, b)\n', iscell(ce), ce{1}, ce{2});
