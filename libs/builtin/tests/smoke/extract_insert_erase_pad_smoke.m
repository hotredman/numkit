clear
import compat.*

% erase / pad / extractAfter / extractBefore / insertAfter / insertBefore
% are class- and shape-preserving (MATLAB R2025b):
%   string array -> string array (same shape)
%   char         -> char
%   cell         -> cell of char vectors

% ── erase ───────────────────────────────────────────────────────
e = erase(["a1b" "c2d"], "1");
fprintf('erase  isstring=%d numel=%d v1=%s v2=%s (expect 1, 2, ab, c2d)\n', ...
        isstring(e), numel(e), e(1), e(2));

% ── pad (explicit width + default = longest element) ────────────
p = pad(["a" "bb"], 4);
fprintf('pad w4  v1=|%s| v2=|%s| (expect "a   ", "bb  ")\n', p(1), p(2));
pn = pad(["a" "bbb"]);
fprintf('pad default  v1=|%s| v2=|%s| (expect "a  ", "bbb")\n', pn(1), pn(2));

% ── extractAfter / extractBefore ────────────────────────────────
ea = extractAfter(["a-b" "c-d"], "-");
fprintf('extractAfter  isstring=%d v1=%s v2=%s (expect 1, b, d)\n', isstring(ea), ea(1), ea(2));
eb = extractBefore(["a-b" "c-d"], "-");
fprintf('extractBefore  v1=%s v2=%s (expect a, c)\n', eb(1), eb(2));

% ── insertAfter / insertBefore ──────────────────────────────────
ia = insertAfter(["ab" "cd"], "a", "X");
fprintf('insertAfter  isstring=%d v1=%s v2=%s (expect 1, aXb, cd)\n', isstring(ia), ia(1), ia(2));
ib = insertBefore(["ab" "cd"], "b", "X");
fprintf('insertBefore  v1=%s v2=%s (expect aXb, cd)\n', ib(1), ib(2));

% ── char + cell paths unchanged ─────────────────────────────────
ec = erase('a1b1c', '1');
fprintf('erase char  ischar=%d v=%s (expect 1, abc)\n', ischar(ec), ec);
pc = pad({'a','bb'}, 3);
fprintf('pad cell  iscell=%d v1=|%s| (expect 1, "a  ")\n', iscell(pc), pc{1});
