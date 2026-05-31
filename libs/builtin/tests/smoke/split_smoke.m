clear
import compat.*

% split — output class mirrors the input class (MATLAB R2025b).
%   string  input -> string array
%   char    input -> cell array of char vectors
% Values and the N-by-1 column shape are the same either way.

% ── string input -> string array ────────────────────────────────
p1 = split("a,b,c", ",");
fprintf('split("a,b,c",",")  isstring=%d  (expect 1)\n', isstring(p1));
fprintf('  numel=%d (expect 3)  p1(1)=%s p1(2)=%s p1(3)=%s (expect a b c)\n', ...
        numel(p1), p1(1), p1(2), p1(3));

% ── char input -> cell array of char vectors ────────────────────
p2 = split('a,b,c', ',');
fprintf('split(''a,b,c'','','')  iscell=%d  (expect 1)\n', iscell(p2));
fprintf('  numel=%d (expect 3)  p2{1}=%s p2{2}=%s p2{3}=%s (expect a b c)\n', ...
        numel(p2), p2{1}, p2{2}, p2{3});

% ── default delimiter is whitespace; string input stays a string ─
p3 = split("one two three");
fprintf('split("one two three")  isstring=%d numel=%d (expect 1, 3)\n', ...
        isstring(p3), numel(p3));

% ── multi-char delimiter, string input ──────────────────────────
p4 = split("a--b--c", "--");
fprintf('split("a--b--c","--")  isstring=%d numel=%d p4(2)=%s (expect 1, 3, b)\n', ...
        isstring(p4), numel(p4), p4(2));
