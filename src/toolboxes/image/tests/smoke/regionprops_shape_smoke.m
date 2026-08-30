clear

% regionprops now reports the moment/area/bbox scalar shape descriptors,
% matching MATLAB R2025b (previously only Area/Centroid/BoundingBox shipped):
%   MajorAxisLength, MinorAxisLength, Eccentricity, Orientation  (ellipse fit
%     from the normalized 2nd central moments, +1/12 per-pixel variance)
%   EquivDiameter = sqrt(4*Area/pi)
%   Extent        = Area / (bbox_width * bbox_height)

% 3x2 block: axis-aligned, vertical major axis -> Orientation = +90.
BW1 = false(6,6); BW1(2:4,2:3) = true;
s1 = regionprops(BW1,'Extent','EquivDiameter','MajorAxisLength', ...
                 'MinorAxisLength','Eccentricity','Orientation');
fprintf('BLOCK  Extent=%.6f EquivD=%.6f Major=%.6f Minor=%.6f Ecc=%.6f Orient=%.4f\n', ...
        s1.Extent, s1.EquivDiameter, s1.MajorAxisLength, s1.MinorAxisLength, s1.Eccentricity, s1.Orientation);
fprintf('  expect Extent=1 EquivD=2.763953 Major=3.464102 Minor=2.309401 Ecc=0.745356 Orient=90\n');

% Diagonal "\" blob: orientation is NEGATIVE (image rows increase downward).
BW2 = false(7,7);
BW2(2,2)=true; BW2(3,2)=true; BW2(3,3)=true; BW2(4,3)=true; BW2(4,4)=true; BW2(5,4)=true;
s2 = regionprops(BW2,'Extent','MajorAxisLength','MinorAxisLength','Eccentricity','Orientation');
fprintf('DIAG   Extent=%.6f Major=%.6f Minor=%.6f Ecc=%.6f Orient=%.4f\n', ...
        s2.Extent, s2.MajorAxisLength, s2.MinorAxisLength, s2.Eccentricity, s2.Orientation);
fprintf('  expect Extent=0.5 Major=4.985233 Minor=1.774106 Ecc=0.934535 Orient=-50.3098\n');

% Single pixel: degenerate ellipse (Major==Minor, Ecc=0, Orient=0).
BW3 = false(3,3); BW3(2,2)=true;
s3 = regionprops(BW3,'MajorAxisLength','MinorAxisLength','Eccentricity','Orientation','EquivDiameter');
fprintf('PIXEL  Major=%.6f Minor=%.6f Ecc=%.6f Orient=%.4f EquivD=%.6f\n', ...
        s3.MajorAxisLength, s3.MinorAxisLength, s3.Eccentricity, s3.Orientation, s3.EquivDiameter);
fprintf('  expect Major=Minor=1.154701 Ecc=0 Orient=0 EquivD=1.128379\n');

% Basic default (no properties) still returns only the basic 3 fields.
sb = regionprops(BW1);
fprintf('basic default fields = %s  (expect Area,BoundingBox,Centroid)\n', strjoin(fieldnames(sb), ','));
