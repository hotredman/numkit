clear

% white — all-ones colormap.

fprintf('--- size(white()) ---\n');
w = white();
fprintf('size: %s  (expect [256 3])\n', mat2str(size(w)));
fprintf('all == 1? %d\n', all(w(:) == 1));

fprintf('\n--- white(5) ---\n');
disp(white(5));

fprintf('\n--- white(0) size ---\n');
fprintf('size white(0)  = %s\n', mat2str(size(white(0))));
