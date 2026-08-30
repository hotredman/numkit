clear

% imadjust treats an EXPLICITLY-passed empty [] for the in/out range as
% MATLAB's default [0 1] -> identity mapping (NO contrast stretch). This is
% DISTINCT from the absent 1-arg form imadjust(I), which auto-stretches via
% stretchlim (1% saturation). numkit previously treated the empty [] like the
% absent case, so imadjust(I,[],[]) wrongly contrast-stretched the image.

I = uint8([10 50 90; 130 170 210; 30 70 110]);

fprintf('--- empty [] in/out range == identity ---\n');
J1 = imadjust(I, [], []);
J2 = imadjust(I, [], [0 1]);
J3 = imadjust(I, [0 1], []);
fprintf('J1==I: %d  J2==I: %d  J3==I: %d   (expect 1 1 1)\n', ...
        isequal(J1, I), isequal(J2, I), isequal(J3, I));
fprintf('J1(1,1)=%d  J1(2,2)=%d   (expect 10, 170 -> untouched)\n', ...
        double(J1(1,1)), double(J1(2,2)));

fprintf('--- 1-arg form still auto-stretches (stretchlim) ---\n');
J4 = imadjust(I);
fprintf('J4==I: %d  J4(1,1)=%d  J4(2,2)=%d   (expect 0, 0, 204)\n', ...
        isequal(J4, I), double(J4(1,1)), double(J4(2,2)));

fprintf('--- explicit range + gamma still applies ---\n');
J5 = imadjust(I, [0.2 0.8], [0 1], 2);
fprintf('J5(2,2)=%d   (expect 154)\n', double(J5(2,2)));

fprintf('--- empty form works for double images too ---\n');
Id = [0.1 0.5 0.9; 0.2 0.6 1.0];
Jd = imadjust(Id, [], []);
fprintf('double J==I: %d   (expect 1)\n', isequal(Jd, Id));
