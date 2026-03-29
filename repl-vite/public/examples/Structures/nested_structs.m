% Nested Structs — Structs inside structs
% Fields can themselves be structs, forming a hierarchy.
clear

car.make = 'Toyota';
car.year = 2024;
car.engine.horsepower = 203;
car.engine.type = 'hybrid';
disp(car.make)
disp(car.engine.horsepower)
