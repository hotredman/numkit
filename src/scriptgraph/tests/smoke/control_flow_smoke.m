clear

% Smoke test for graph-lowering Phase 2 — exercises every control-flow
% construct the lowering pass handles so the IDE's graph view has a
% realistic fixture to render, and `numkit.exe` confirms the
% script also runs at runtime end-to-end.
%
% Constructs exercised:
%   • if / elseif / else        (Phase 2a regions + 2c merge nodes)
%   • switch / case / otherwise (Phase 2a + 2c merge)
%   • try / catch               (Phase 2a + 2c merge)
%   • for with loop-carried var (Phase 2e loop-φ)
%   • while with loop-carried   (Phase 2e loop-φ)
%   • nested loops              (Phase 2e chained φ)
%   • break / continue          (Phase 2b jump edges)
%   • function definition       (Phase 2d FunctionDef region)
%   • function with return mid-body (Phase 2b + 2d)

fprintf('=== Control-flow smoke ===\n');

% ── if / elseif / else  +  merge node ──────────────────────────────
x = 5;
if x > 0
    sign_x = 1;
elseif x < 0
    sign_x = -1;
else
    sign_x = 0;
end
fprintf('if/elseif/else: sign(%g) = %g (expect 1)\n', x, sign_x);

% ── switch / case / otherwise  +  merge ────────────────────────────
op = 'add';
switch op
    case 'add', r = 2 + 3;
    case 'sub', r = 2 - 3;
    otherwise,  r = 0;
end
fprintf('switch: op=%s result=%g (expect 5)\n', op, r);

% ── try / catch  +  merge ──────────────────────────────────────────
try
    z = sqrt(-1);            % no error in MATLAB → complex
    err_msg = 'none';
catch ME
    err_msg = ME.message;
end
fprintf('try/catch: error=%s (expect none)\n', err_msg);

% ── for with loop-carried `s`  ─────────────────────────────────────
s = 0;
for k = 1:5
    s = s + k;               % `s` is loop-carried → φ at header
end
fprintf('for loop-carried: sum 1..5 = %g (expect 15)\n', s);

% ── while with loop-carried `n` ─────────────────────────────────────
n = 1;
while n < 100
    n = n * 2;
end
fprintf('while loop-carried: first 2^k >= 100 = %g (expect 128)\n', n);

% ── nested loops sharing `m` (chained φ) ───────────────────────────
m = 0;
for i = 1:3
    for j = 1:4
        m = m + i * j;
    end
end
fprintf('nested loops: m = %g (expect 60)\n', m);

% ── for + break ────────────────────────────────────────────────────
found = -1;
for i = 1:100
    if i * i > 50
        found = i;
        break;
    end
end
fprintf('break: first i with i^2 > 50 = %d (expect 8)\n', found);

% ── for + continue ─────────────────────────────────────────────────
sum_odd = 0;
for k = 1:10
    if mod(k, 2) == 0
        continue;
    end
    sum_odd = sum_odd + k;
end
fprintf('continue: sum of odd 1..10 = %g (expect 25)\n', sum_odd);

% ── function calls (functions defined at file end per MATLAB rule) ─
fprintf('double_it(7) = %g (expect 14)\n', double_it(7));
fprintf('clamp(-0.5)  = %g (expect 0)\n',   clamp(-0.5));
fprintf('clamp(0.3)   = %g (expect 0.3)\n', clamp(0.3));
fprintf('clamp(1.5)   = %g (expect 1)\n',   clamp(1.5));

[a, b] = swap_pair(10, 20);
fprintf('swap(10,20)  = [%g %g] (expect [20 10])\n', a, b);

fprintf('=== Smoke OK ===\n');

% ── function defs ──────────────────────────────────────────────────
% MATLAB requires function definitions to live at the END of a script.

function y = double_it(x)
    y = 2 * x;
end

function r = clamp(v)
    % Function with early returns — exercises Phase 2b `return` jump
    % edge inside a Phase 2d FunctionDef region.
    if v < 0
        r = 0;
        return;
    end
    if v > 1
        r = 1;
        return;
    end
    r = v;
end

function [a, b] = swap_pair(x, y)
    a = y;
    b = x;
end
