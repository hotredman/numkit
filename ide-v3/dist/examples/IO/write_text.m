% Write Text — fopen, fprintf, fclose
% Write a few formatted lines to a text file and close it.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

fid = fopen('greetings.txt', 'w');
fprintf(fid, 'Hello %s!\n', 'world');
fprintf(fid, 'Today is day %d.\n', 42);
fprintf(fid, 'pi = %.4f\n', pi);
fclose(fid);

disp('Wrote greetings.txt')
