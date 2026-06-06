% MAT Multiple Variables — save a workspace bundle
% One .mat file can hold any number of variables of mixed types:
% numeric, complex, char, logical, cell, struct. load() without an
% LHS reinjects each one into the workspace under its stored name;
% S = load(...) packs them all as fields of a struct instead.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

x        = linspace(0, 2*pi, 8);
y        = sin(x);
z        = [1+2i, 3-4i];
label    = 'sine sample';
meta.fs  = 8000;
meta.tag = 'demo';

save('bundle.mat', 'x', 'y', 'z', 'label', 'meta');

% Form 1 — variables go straight into the workspace.
clear x y z label meta
load('bundle.mat');
fprintf('y(4)        = %.4f\n', y(4));
fprintf('label       = %s\n',   label);
fprintf('meta.fs     = %d\n',   meta.fs);

% Form 2 — capture everything as fields of one struct.
S = load('bundle.mat');
names = fieldnames(S);
for k = 1:numel(names)
    fprintf('S.%s\n', names{k});
end
