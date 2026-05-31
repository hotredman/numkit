clear

import compat.*

% IIR designers 3-output [z,p,k] (DEEP-PROBE 2026-05-31). MATLAB lets you
% request the digital zero/pole/gain form: [z,p,k] = butter/cheby1/cheby2/
% ellip(...). numkit previously errored ("Undefined function or variable
% 'k'") because only the 2-output [b,a] form was wired. The denominator is
% monic so the ZPK gain equals b(1); z/p are recovered via tf2zp. vs MATLAB
% R2025b. (Poles + gain exact for all; finite zeros exact for cheby2/ellip;
% the all-pole butter/cheby1 lowpass zeros sit at z=-1 and are root-found.)

fprintf('=== butter(4,0.3) [z,p,k] ===\n');
[z,p,k] = butter(4,0.3);
fprintf('numel(z)=%d numel(p)=%d k=%.10g  (expect 4 4 0.0185630106)\n', numel(z), numel(p), k);
fprintf('sum(real(p))=%.8g  (expect 1.57039885)\n', sum(real(p)));

fprintf('\n=== cheby1(4,1,0.3) ===\n');
[z,p,k] = cheby1(4,1,0.3);
fprintf('numel(z)=%d numel(p)=%d k=%.10g sum(real(p))=%.8g  (expect 4 4 0.00836323956 2.37412317)\n', ...
        numel(z), numel(p), k, sum(real(p)));

fprintf('\n=== cheby2(4,30,0.3) (distinct zeros) ===\n');
[z,p,k] = cheby2(4,30,0.3);
fprintf('k=%.10g sum(real(p))=%.8g sum(real(z))=%.8g  (expect 0.0470498339 2.26899171 0.509710648)\n', ...
        k, sum(real(p)), sum(real(z)));

fprintf('\n=== ellip(4,1,30,0.3) (distinct zeros) ===\n');
[z,p,k] = ellip(4,1,30,0.3);
fprintf('k=%.10g sum(real(p))=%.8g sum(real(z))=%.8g  (expect 0.0647314906 2.28002378 0.16390207)\n', ...
        k, sum(real(p)), sum(real(z)));

fprintf('\n=== [b,a] 2-output still works (round-trips to z,p,k) ===\n');
[b,a] = butter(4,0.3);
fprintf('butter [b,a]: numel(b)=%d numel(a)=%d b(1)=%.10g  (b(1)=k, expect 0.0185630106)\n', numel(b), numel(a), b(1));
