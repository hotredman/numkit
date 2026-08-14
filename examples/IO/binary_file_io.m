% Binary Stream I/O — fopen, fwrite, fseek, fread, fclose
% Demonstrates raw low-level binary serialization and deserialization
% using C-style file streams in NumKit.

clear

% 1. Create binary test data
data = linspace(1.0, 10.0, 10);

% 2. Open file for writing in binary mode
fid = fopen('raw_data.bin', 'wb');
if fid < 0
    error('Could not open raw_data.bin for writing');
end

% Write as 64-bit IEEE double-precision floats
count = fwrite(fid, data, 'double');
fprintf('Wrote %d double elements to binary file\n', count);
fclose(fid);

% 3. Open file for reading
fid = fopen('raw_data.bin', 'rb');
if fid < 0
    error('Could not open raw_data.bin for reading');
end

% Seek to the 5th element (offset in bytes = 4 * 8 bytes)
fseek(fid, 4 * 8, 'bof');
pos = ftell(fid);
fprintf('Stream position after seek: %d bytes\n', pos);

% Read 3 elements starting from 5th element
read_subset = fread(fid, 3, 'double');
disp('Read subset (elements 5, 6, 7):');
disp(read_subset');

% Rewind and read all elements
fseek(fid, 0, 'bof');
all_data = fread(fid, Inf, 'double');
fclose(fid);

disp('All data read back:');
disp(all_data');
