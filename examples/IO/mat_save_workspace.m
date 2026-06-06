% MAT Save Whole Workspace — save(filename) with no varnames
% Calling save with only a filename writes every variable in the
% current workspace. Handy as a "checkpoint": dump state, do
% something destructive, restore.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

A     = reshape(1:12, 3, 4);
B     = eye(3);
note  = 'checkpoint before sort';

save('checkpoint.mat');     % no varnames → whole workspace

% Mutate state...
A = sort(A(:));
B = B + 99;
note = 'after sort';

% ...then restore.
load('checkpoint.mat');
disp(A)
disp(note)
