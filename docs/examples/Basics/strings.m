% Strings — String operations
% Upper/lower case conversion, length, and comparison.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

disp(upper('hello'))
disp(lower('MATLAB'))
disp(length('OpenAI'))
disp(strcmp('test', 'test'))
