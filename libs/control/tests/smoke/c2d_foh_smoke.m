clear
import compat.*

% c2d 'foh' — first-order (triangle) hold, vs MATLAB R2025b.
sys = tf(1, [1 2 1]);
Ts = 0.1;

dz = c2d(sys, Ts, 'zoh');  [bz, az] = tfdata(dz, 'v');
df = c2d(sys, Ts, 'foh');  [bf, af] = tfdata(df, 'v');
fprintf('zoh b = %s\n', mat2str(bz, 8));   % [0 0.0046788 0.0043771]
fprintf('foh b = %s\n', mat2str(bf, 8));   % [0.0015858 0.0060353 0.0014349]
fprintf('foh a = %s\n', mat2str(af, 8));   % [1 -1.8096748 0.81873075]
fprintf('denominators equal: %d\n', max(abs(az - af)) < 1e-12);  % 1 (same poles)

% State-space form keeps Ad = ZOH Ad; FOH modifies B and adds feedthrough D.
sc = ss([-2 -1; 1 0], [1; 0], [0 1], 0);
dd = c2d(sc, Ts, 'foh');
fprintf('ss Dd = %.10g (nonzero feedthrough)\n', dd.D);   % 0.0015857788
