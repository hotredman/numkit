clear
import compat.*

% lower / upper / reverse are class-preserving (MATLAB R2025b):
%   string array -> string array (same shape)
%   string scalar -> string scalar
%   char         -> char
%   cell         -> cell of char vectors

% ── string array -> string array ────────────────────────────────
lo = lower(["AB" "CD"]);
fprintf('lower(["AB" "CD"])  isstring=%d numel=%d v1=%s v2=%s (expect 1, 2, ab, cd)\n', ...
        isstring(lo), numel(lo), lo(1), lo(2));
up = upper(["ab" "cd"]);
fprintf('upper(["ab" "cd"])  isstring=%d v1=%s v2=%s (expect 1, AB, CD)\n', ...
        isstring(up), up(1), up(2));
rv = reverse(["abc" "de"]);
fprintf('reverse(["abc" "de"]) isstring=%d numel=%d v1=%s v2=%s (expect 1, 2, cba, ed)\n', ...
        isstring(rv), numel(rv), rv(1), rv(2));

% ── column string array: shape preserved ────────────────────────
cs = upper(["a"; "b"]);
fprintf('upper col str-arr  sz=%dx%d (expect 2x1)\n', size(cs,1), size(cs,2));

% ── string scalar -> string scalar (NOT char) ───────────────────
ls = lower("HeLLo");
fprintf('lower("HeLLo")  isstring=%d v=%s (expect 1, hello)\n', isstring(ls), ls);

% ── char input -> char (unchanged) ──────────────────────────────
lc = upper('world');
fprintf('upper(''world'')  ischar=%d v=%s (expect 1, WORLD)\n', ischar(lc), lc);

% ── cell input -> cell of char vectors (unchanged) ──────────────
ce = lower({'AB', 'CD'});
fprintf('lower cell  iscell=%d v1=%s v2=%s (expect 1, ab, cd)\n', iscell(ce), ce{1}, ce{2});
