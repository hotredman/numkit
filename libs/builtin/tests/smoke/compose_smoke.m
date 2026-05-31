clear
import compat.*

% compose — applies the format repeatedly across each ROW of x, consuming
% M = (number of conversion specs) values per output element. An R-by-C
% input yields an R-by-ceil(C/M) result. A short trailing value chunk
% leaves the unfilled specs as literal text. The output class mirrors the
% FORMAT class: char fmt -> cell of char vectors, string fmt -> string array.

% ── divisible multi-spec: per-row pairing ───────────────────────
A = compose('%d-%d', [1 2; 3 4]);
fprintf('compose(''%%d-%%d'',[1 2;3 4])  iscell=%d numel=%d sz=%dx%d\n', ...
        iscell(A), numel(A), size(A,1), size(A,2));
fprintf('  A{1}=%s  A{2}=%s   (expect 1, 2, 2x1, "1-2", "3-4")\n', A{1}, A{2});

% ── one spec over a 2x2 -> 2x2, column-major ────────────────────
B = compose('%d', [1 2; 3 4]);
fprintf('compose(''%%d'',[1 2;3 4])  numel=%d sz=%dx%d  B{3}=%s (expect 4, 2x2, "2")\n', ...
        numel(B), size(B,1), size(B,2), B{3});

% ── row vector, 2 specs -> 1x2 ──────────────────────────────────
C = compose('%d-%d', [1 2 3 4]);
fprintf('compose(''%%d-%%d'',[1 2 3 4])  numel=%d  C{1}=%s C{2}=%s (expect 2, "1-2", "3-4")\n', ...
        numel(C), C{1}, C{2});

% ── non-divisible: trailing spec stays literal "%d" ─────────────
H = compose('%d-%d', [1 2 3; 4 5 6]);
fprintf('compose(''%%d-%%d'',[1 2 3;4 5 6])  numel=%d  H{3}=%s H{4}=%s (expect 4, "3-%%d", "6-%%d")\n', ...
        numel(H), H{3}, H{4});

% ── string format -> string array ───────────────────────────────
S = compose("%d-%d", [1 2; 3 4]);
fprintf('compose("%%d-%%d",[1 2;3 4])  isstring=%d  S(1)=%s (expect 1, "1-2")\n', ...
        isstring(S), S(1));
