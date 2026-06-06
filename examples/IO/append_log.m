% Append Log — fopen with 'a'
% 'a' preserves existing content and writes past the end of the file.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

% Start the log fresh
fid = fopen('run.log', 'w');
fprintf(fid, 'session start\n');
fclose(fid);

% Append more entries in separate fopen/fclose cycles
fid = fopen('run.log', 'a');
fprintf(fid, 'step 1 OK\n');
fprintf(fid, 'step 2 OK\n');
fclose(fid);

disp('Appended to run.log')
