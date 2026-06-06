% Nested Structs — Structs inside structs
% Fields can themselves be structs, forming a hierarchy.
% Pull toolbox functions (signal, stats, graphics, io) into scope so we
% can call them by short name (fft, butter, plot, std, ...). Without this,
% we'd need fully qualified names like signal.transforms.fft(...).
import compat.*;

clear

car.make = 'Toyota';
car.year = 2024;
car.engine.horsepower = 203;
car.engine.type = 'hybrid';
disp(car.make)
disp(car.engine.horsepower)
