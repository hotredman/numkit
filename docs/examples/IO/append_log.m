% Append Log — fopen with 'a'
% 'a' preserves existing content and writes past the end of the file.
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
