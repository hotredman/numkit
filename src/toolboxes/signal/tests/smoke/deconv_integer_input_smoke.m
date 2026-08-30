clear

% bugs/signal/deconv-integer-input.md — deconv accepts integer/logical input.
% MATLAB R2025b promotes to double; the quotient AND remainder are ALWAYS
% double (never the integer class, like conv). Previously numkit threw
% "Not a double array" on any integer or logical operand.

q1 = deconv(int8([1 3 5 3]), int8([1 1]));
fprintf('deconv(int8,int8) q = [%g %g %g] class=%s   expect [1 2 3] double\n', q1(1), q1(2), q1(3), class(q1));

q2 = deconv(int16([2 7 7 2]), int16([1 2]));
fprintf('deconv(int16,int16) q = [%g %g %g] class=%s   expect [2 3 1] double\n', q2(1), q2(2), q2(3), class(q2));

qm = deconv([1 3 5 3], int8([1 1]));
fprintf('deconv(double,int8) q = [%g %g %g] class=%s   expect [1 2 3] double (mixed)\n', qm(1), qm(2), qm(3), class(qm));

ql = deconv(logical([1 0 1 0]), [1 1]);
fprintf('deconv(logical,double) q = [%g %g %g] class=%s   expect [1 -1 2] double\n', ql(1), ql(2), ql(3), class(ql));

[q3, r3] = deconv(int8([1 3 5 3]), int8([1 1]));
fprintf('[q,r]=deconv(int8): q(2)=%g, max|r|=%g, class_r=%s   expect 2 / 0 / double\n', q3(2), max(abs(r3)), class(r3));

% Divisor longer than dividend (na > nb), DOUBLE: q is scalar 0, r = numerator.
[q5, r5] = deconv([1 1], [1 2 3]);
fprintf('na>nb double: q=%g (scalar 0), r=[%g %g], class_q=%s   expect 0 / [1 1] / double\n', q5(1), r5(1), r5(2), class(q5));

% numkit-lenient extension: na>nb with INTEGER input — MATLAB ERRORS here
% ("Inputs must be floats"); numkit promotes and returns double instead.
[q4, r4] = deconv(int8([1 1]), int8([1 2 3]));
fprintf('na>nb int (numkit lenient; MATLAB errors): q=%g r=[%g %g] class=%s\n', q4(1), r4(1), r4(2), class(q4));

% Regression: plain double/double unchanged.
qd = deconv([2 7 7 2], [1 2]);
fprintf('deconv(double,double) q = [%g %g %g] class=%s   expect [2 3 1] double\n', qd(1), qd(2), qd(3), class(qd));
