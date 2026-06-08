clear;
import compat.*;

% odeset — build/merge options struct.

fprintf('--- 1) odeset() with no args returns defaults ---\n');
o0 = odeset();
fprintf('isstruct=%d  RelTol=[%s]  AbsTol=[%s]\n', ...
        isstruct(o0), mat2str(o0.RelTol), mat2str(o0.AbsTol));

fprintf('\n--- 2) odeset(name, value, ...) ---\n');
o1 = odeset('RelTol', 1e-9, 'AbsTol', 1e-12, 'MaxStep', 0.05, 'Refine', 6);
fprintf('RelTol=%.0e AbsTol=%.0e MaxStep=%.2f Refine=%d\n', ...
        o1.RelTol, o1.AbsTol, o1.MaxStep, o1.Refine);

fprintf('\n--- 3) case-insensitive name lookup (canonical capitalisation) ---\n');
o2 = odeset('reltol', 1e-7, 'abstol', 1e-9, 'normcontrol', 'on');
fprintf('RelTol=%.0e AbsTol=%.0e NormControl=%s\n', ...
        o2.RelTol, o2.AbsTol, o2.NormControl);

fprintf('\n--- 4) odeset(oldstruct, name, value, ...) merges ---\n');
o3 = odeset(o1, 'RelTol', 1e-6, 'OutputFcn', @disp);
fprintf('RelTol now=%.0e (was 1e-9)  AbsTol still=%.0e  has OutputFcn=%d\n', ...
        o3.RelTol, o3.AbsTol, ~isempty(o3.OutputFcn));
