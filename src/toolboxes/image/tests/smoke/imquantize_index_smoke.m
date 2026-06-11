clear
import compat.*

% imquantize now returns the second output (the quantization INDEX) and
% accepts the third 'values' argument. The index is the 1-based bin number
% (1..numel(levels)+1) for each pixel. quant = values(index) when 'values'
% is supplied, otherwise quant == index. numkit previously returned only the
% index as 'quant' and threw "Index exceeds array dimensions" on [q, idx].

I = [1 5 10; 15 20 25];

fprintf('--- [quant, index] without values (quant == index) ---\n');
[Q, idx] = imquantize(I, [8 18]);
fprintf('Q==idx? %d   Q(1,1)=%d Q(2,2)=%d   idx(2,3)=%d   (expect 1, 1, 3, 3)\n', ...
        isequal(Q, idx), double(Q(1,1)), double(Q(2,2)), double(idx(2,3)));

fprintf('--- with a value table: quant = values(index) ---\n');
[Qv, idxv] = imquantize(I, [8 18], [10 20 30]);
fprintf('Qv(1,1)=%d  Qv(2,2)=%d   (expect 10, 30)\n', double(Qv(1,1)), double(Qv(2,2)));
fprintf('idxv stays the bin index: idxv==idx? %d\n', isequal(idxv, idx));

fprintf('--- single-output form unchanged ---\n');
q1 = imquantize(I, [8 18]);
fprintf('imquantize(I,levels) == idx? %d\n', isequal(q1, idx));
