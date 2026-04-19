% tests/compat/matlab_parity_checks.m
%
% Run in real MATLAB with a fresh session:
%
%     diary matlab_parity.log
%     matlab_parity_checks
%     diary off
%
% Each section clears the workspace first, prints a marker, runs the
% check, and displays the relevant output. The log file can then be
% diff'd against numkit-m's REPL output to identify deviations.
%
% Sections H1..H3 require the debugger — see the note at the end.

%% ============================================================
%% A. `ans` and the semicolon
%% ============================================================

fprintf('\n=== A1: bare expression, no semicolon ===\n');
clear all; %#ok<CLALL>
5 + 3
whos

fprintf('\n=== A2: bare expression with semicolon ===\n');
clear all;
5 + 3;
whos

fprintf('\n=== A3: function call with semicolon ===\n');
clear all;
sin(0.5);
whos


%% ============================================================
%% B. Shadowing built-ins (REPL / workspace visibility)
%% ============================================================

fprintf('\n=== B1: read-only pi ===\n');
clear all;
pi
whos

fprintf('\n=== B2: shadow pi by assignment ===\n');
clear all;
pi = 5
whos

fprintf('\n=== B3: clear pi un-shadows ===\n');
clear all;
pi = 5;
fprintf('after pi = 5: '); disp(pi)
clear pi
fprintf('after clear pi: '); disp(pi)
whos

fprintf('\n=== B4: reading pi into x should NOT list pi ===\n');
clear all;
x = pi;
whos


%% ============================================================
%% C. Pseudo-vars inside a function
%% Requires a file `probe.m` next to this script with:
%%     function r = probe(a, b, c)
%%         whos
%%         r = a + b + c;
%%     end
%% ============================================================

fprintf('\n=== C1: whos inside probe(1,2,3) — does it list nargin/nargout? ===\n');
clear all;
if exist('probe', 'file') == 2
    probe(1, 2, 3);
else
    fprintf('SKIP — create probe.m (contents in the comment above)\n');
end


%% ============================================================
%% D. `clear` variants
%% ============================================================

fprintf('\n=== D1: clear <name> removes just that var ===\n');
clear all;
a = 1; b = 2; c = 3;
clear b
whos

fprintf('\n=== D2: bare clear removes everything ===\n');
clear all;
a = 1; b = 2;
clear
whos

fprintf('\n=== D3: clear of nonexistent name ===\n');
clear all;
try
    clear nonexistent_xyz
    fprintf('no error\n');
catch err
    fprintf('error: %s\n', err.message);
end


%% ============================================================
%% E. Shadow + clear round-trip for a built-in
%% ============================================================

fprintf('\n=== E1: shadow eps, disp, clear, disp ===\n');
clear all;
eps = 99;
fprintf('after eps = 99: '); disp(eps)
clear eps
fprintf('after clear eps: '); disp(eps)


%% ============================================================
%% F. who vs whos formatting
%% ============================================================

fprintf('\n=== F1: who and whos formatting ===\n');
clear all;
x = 42;
big = zeros(100, 100);
fprintf('--- who ---\n');
who
fprintf('--- whos ---\n');
whos


%% ============================================================
%% G. Assignment-like forms that mutate a variable
%% ============================================================

fprintf('\n=== G1: v(idx) = [] must keep v in workspace ===\n');
clear all;
v = [1 2 3 4];
v(2) = [];
v
whos

fprintf('\n=== G2: nested struct field assignment ===\n');
clear all;
s.a.b = 1;
whos

fprintf('\n=== G3: cell assignment ===\n');
clear all;
c = {1, 2, 3};
c{2} = 99;
whos


%% ============================================================
%% I. Pseudo-vars at the base workspace
%% ============================================================

fprintf('\n=== I1: nargin outside a function ===\n');
clear all;
try
    nargin
catch err
    fprintf('error: %s\n', err.message);
end


%% ============================================================
%% J. Special constants and literal forms
%% ============================================================

fprintf('\n=== J1: i, j, true, false ===\n');
clear all;
disp(i)
disp(j)
disp(true)
disp(false)

fprintf('\n=== J2: shadow i, then use 1i literal ===\n');
clear all;
i = 5
disp(1i)      % should still be imaginary unit, not 5 * 1
fprintf('whos after shadowing i:\n');
whos


%% ============================================================
%% H. Debugger checks — run MANUALLY after this script finishes.
%% ============================================================
%
%   1. Create `dbg_script.m`:
%
%          x = 10;
%          y = 20;
%          z = x + y;
%          disp(z)
%
%   2. In the command window:
%
%          dbstop in dbg_script at 2
%          dbg_script
%
%   3. When paused (K>>):
%
%       H1)  whos
%            % Expected: x visible; y visible or not depending on pre/post.
%            %           z must NOT be visible yet.
%
%       H2)  pi = 5
%            whos
%            % Expected: pi appears in the list after the assignment.
%
%       H3)  clear x
%            dbcont
%            % Expected: runtime error "Undefined function or variable 'x'"
%            %           at the `z = x + y` line.
%
%   4. `dbclear all` to clean up.
%
% Paste the full K>> transcript for H1..H3 into the log so it can be
% compared against numkit-m's debug session output.
%
%% End of script.
