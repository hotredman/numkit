clear
import compat.*

fprintf('=== cdf2rdf / rsf2csf ===\n');

% cdf2rdf — build complex (V, D) by hand for A = [0 -1; 1 0].
A = [0 -1; 1 0];
V_c = [complex(1,0) complex(1,0); complex(0,-1) complex(0,1)] / sqrt(2);
D_c = [complex(0,1) 0; 0 complex(0,-1)];
[VR, DR] = cdf2rdf(V_c, D_c);
fprintf('cdf2rdf:\n');
fprintf('  DR = [%.3f %.3f; %.3f %.3f]   (MATLAB convention [a b; -b a])\n', DR(1,1), DR(1,2), DR(2,1), DR(2,2));
fprintf('  ||A - VR*DR*inv(VR)|| = %.3e\n', norm(A - VR*DR*inv(VR), 'fro'));

% rsf2csf — real-Schur 2x2 block → complex upper-tri.
T_real = [0.5 -1.5; 1.5 0.5];
[Uc, Tc] = rsf2csf(eye(2), T_real);
fprintf('\nrsf2csf:\n');
fprintf('  Tc(1,1) = %.4f%+.4fi   (expect 0.5+/-1.5i)\n', real(Tc(1,1)), imag(Tc(1,1)));
fprintf('  Tc(2,2) = %.4f%+.4fi\n', real(Tc(2,2)), imag(Tc(2,2)));
fprintf('  |Tc(2,1)| = %.3e  (expect ~0, upper-triangular)\n', abs(Tc(2,1)));

% Real-only inputs are no-ops (cdf2rdf with real D, rsf2csf with strictly upper T).
fprintf('\nreal-only passthrough:\n');
[VR2, DR2] = cdf2rdf([complex(1,0) complex(0,0); complex(0,0) complex(1,0)], ...
                     [complex(2,0) 0; 0 complex(3,0)]);
fprintf('  cdf2rdf(real V, real D): DR = diag(%.0f, %.0f)\n', DR2(1,1), DR2(2,2));
