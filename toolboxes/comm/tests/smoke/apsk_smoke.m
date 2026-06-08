clear
import compat.*

fprintf('=== apskmod / apskdemod (multi-ring constellation) ===\n');

% Standard 16-APSK with identity mapping (matches MATLAB-natural CCW order)
M = [4 12];
radii = [1 2.7];
x = (0:15)';
mapping = (0:15);

y = apskmod(x, M, radii, pi./M, mapping);
fprintf('  16-APSK constellation (identity mapping):\n');
for k = 1:length(y)
    fprintf('    y(%2d) = %+.6f%+.6fi  (r=%5.3f, ang=%7.2f deg)\n', ...
            k-1, real(y(k)), imag(y(k)), abs(y(k)), ...
            atan2(imag(y(k)), real(y(k))) * 180 / pi);
end
fprintf('  Expect ring 0 (M=4) at radii=1, angles 45/135/225/315\n');
fprintf('  Expect ring 1 (M=12) at radii=2.7, angles 15+30k for k=0..11\n');

% Round-trip
z = apskdemod(y, M, radii, pi./M, mapping);
fprintf('\n  round-trip: ');
fprintf('%d ', z);
fprintf(' (expect 0..15)\n');

% Default phase offset (pi./M) when omitted
y2 = apskmod(x, M, radii, [], mapping);
match = isequal(y, y2);
fprintf('\n  default phaseoffset matches explicit pi./M: %d (expect 1)\n', match);
