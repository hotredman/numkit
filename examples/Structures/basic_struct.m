% Basic Struct — Create and access struct fields
% Structs group related data under named fields.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

person.name  = 'Alice';
person.age   = 30;
person.score = [95 87 92];
disp(person.name)
disp(person.age)
disp(person.score)
