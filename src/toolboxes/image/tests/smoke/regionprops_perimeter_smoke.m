clear

% regionprops 'Perimeter' (bugs/image/regionprops-perimeter) — used to be
% silently dropped; now traces the outer 8-connected boundary and applies
% MATLAB's Vossepoel-Smeulders weighted estimator (matches MATLAB R2025b).

s = regionprops(logical(ones(3,3)), 'Perimeter');
fprintf('3x3 solid : %.4f   (expect 7.4760)\n', s.Perimeter);
s = regionprops(logical(ones(4,4)), 'Perimeter');
fprintf('4x4 solid : %.4f   (expect 11.3960)\n', s.Perimeter);
s = regionprops(logical([0 1 0; 1 1 1; 0 1 0]), 'Perimeter');
fprintf('plus      : %.4f   (expect 5.6240)\n', s.Perimeter);
s = regionprops(logical(eye(4)), 'Perimeter');
fprintf('diag eye4 : %.4f   (expect 8.4360)\n', s.Perimeter);

% A property numkit does not implement now errors clearly instead of being
% silently dropped (which used to surface as a confusing field-access error).
try
    regionprops(logical(ones(3,3)), 'Solidity');
    fprintf('Solidity  : NO ERROR (unexpected)\n');
catch e
    fprintf('Solidity  : errors clearly (expected)\n');
end
