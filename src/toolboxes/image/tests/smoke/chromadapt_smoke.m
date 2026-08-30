clear

% chromadapt — Bradford / von Kries / Simple chromatic adaptation.
A = uint8(reshape(linspace(20, 240, 48), [4 4 3]));
ill = [220 200 160];

fprintf('=== bradford (default) ===\n');
B = chromadapt(A, ill);
fprintf('class=%s  B(2,2,:)=[%d %d %d] (expect [0 117 241])\n', class(B), B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== vonkries ===\n');
B = chromadapt(A, ill, 'Method', 'vonkries');
fprintf('B(2,2,:)=[%d %d %d] (expect [46 107 240])\n', B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== simple ===\n');
B = chromadapt(A, ill, 'Method', 'simple');
fprintf('B(2,2,:)=[%d %d %d] (expect [39 119 247])\n', B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== ColorSpace=linear-rgb ===\n');
B = chromadapt(A, ill, 'ColorSpace', 'linear-rgb');
fprintf('B(2,2,:)=[%d %d %d] (expect [34 118 239])\n', B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== ColorSpace=adobe-rgb-1998 ===\n');
B = chromadapt(A, ill, 'ColorSpace', 'adobe-rgb-1998');
fprintf('B(2,2,:)=[%d %d %d] (expect [0 118 241])\n', B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== ColorSpace=prophoto-rgb ===\n');
B = chromadapt(A, ill, 'ColorSpace', 'prophoto-rgb');
fprintf('B(2,2,:)=[%d %d %d] (expect [60 123 248])\n', B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== White illuminant (no-op) ===\n');
B = chromadapt(A, [255 255 255]);
d = double(B) - double(A);
fprintf('max(abs(B - A)) = %d (expect 0 or near)\n', max(abs(d(:))));

fprintf('\n=== single input ===\n');
As = single(A)/255;
B = chromadapt(As, ill);
fprintf('class=%s B(2,2,:)=[%.4f %.4f %.4f]\n', class(B), B(2,2,1), B(2,2,2), B(2,2,3));

fprintf('\n=== double input ===\n');
Ad = double(A)/255;
B = chromadapt(Ad, ill);
fprintf('class=%s B(2,2,:)=[%.6f %.6f %.6f]\n', class(B), B(2,2,1), B(2,2,2), B(2,2,3));
