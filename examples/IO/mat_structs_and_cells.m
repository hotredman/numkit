% MAT Structs and Cells — save complex hierarchical data
% Demonstrates saving and restoring complex data structures:
% scalar structs, struct arrays, cell arrays, and nested hierarchies in Level 5 MAT files.

clear

% 1. Create a scalar struct with diverse field types
config.name       = 'Simulation Experiment #42';
config.sampleRate = 44100;
config.gains      = [1.0, 0.707, 0.5];
config.enabled    = true;

% 2. Create a struct array
sensors(1).id   = 101;
sensors(1).loc  = [0.0, 1.5];
sensors(1).desc = 'Front Radar';

sensors(2).id   = 102;
sensors(2).loc  = [2.0, -1.0];
sensors(2).desc = 'Rear Lidar';

% 3. Create a cell array with mixed types
dataset = { 'batch_01', randn(3, 3), { 'nested_tag', 999 } };

% 4. Save all structures to .mat file
filename = 'complex_data.mat';
save(filename, 'config', 'sensors', 'dataset');

% 5. Clear and restore
clear config sensors dataset
S = load(filename);

fprintf('Experiment: %s\n', S.config.name);
fprintf('Sensors count: %d\n', numel(S.sensors));
fprintf('Sensor 2: ID=%d, Desc=%s\n', S.sensors(2).id, S.sensors(2).desc);
fprintf('Dataset tag: %s, value=%d\n', S.dataset{3}{1}, S.dataset{3}{2});
