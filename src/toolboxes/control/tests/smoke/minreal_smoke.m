clear

% Minimal realization: cancel pole/zero pairs. bugs/control/minreal.
% tf/zpk -> root cancellation; SISO ss -> ss2tf -> cancel -> tf2ss.
% MATLAB pads num to den length on tfdata extraction.

function show(tag, sys)
    [n, d] = tfdata(sys, 'v');
    fprintf('%s: num=[%s] den=[%s]\n', tag, num2str(n, '%.6g '), num2str(d, '%.6g '));
end

show('(s+1)/(s+1)^2   -> 1/(s+1)',  minreal(tf([1 1], [1 2 1])));        % [0 1]/[1 1]
show('(s+1)/((s+1)(s+2)) -> 1/(s+2)', minreal(tf([1 1], [1 3 2])));      % [0 1]/[1 2]
show('2(s+1)/(s+1)^2 -> 2/(s+1)',   minreal(tf(2*[1 1], [1 2 1])));      % [0 2]/[1 1]
show('no cancel 1/(s+1)',           minreal(tf(1, [1 1])));              % [0 1]/[1 1]
show('complex (s^2+1) cancels',     minreal(tf([1 0 1], conv([1 0 1], [1 3])))); % [0 1]/[1 3]

% SISO ss with an uncontrollable mode: order 2 -> 1.
A = [-1 0; 0 -2]; B = [1; 0]; C = [1 1]; D = 0;
sysr = minreal(ss(A, B, C, D));
fprintf('ss order: before=2  after=%d   (expect 1)\n', size(sysr.A, 1));
[n, d] = tfdata(sysr, 'v');
fprintf('ss->tf: num=[%s] den=[%s]   (expect [0 1]/[1 1])\n', ...
        num2str(n, '%.4g '), num2str(d, '%.4g '));
