clear

% dot on COMPLEX vectors: MATLAB conjugates the FIRST argument,
% dot(a,b) = sum(conj(a).*b). numkit previously dropped the imaginary part
% (returned a wrong real scalar). MATLAB R2025b.

fprintf('dot([1+2i 3],[4 5i]) = %s (expect 4+7i)\n', mat2str(dot([1+2i 3], [4 5i])));
fprintf('dot([1 2],[1i 2i])   = %s (expect 5i)\n', mat2str(dot([1 2], [1i 2i])));
disp('dot([1+1i 2;3 4i],[1 1i;1 1]) (expect [4-1i -2i]):');
disp(dot([1+1i 2; 3 4i], [1 1i; 1 1]));

% real path unchanged.
fprintf('dot([1 2 3],[4 5 6]) = %g (expect 32)\n', dot([1 2 3], [4 5 6]));
