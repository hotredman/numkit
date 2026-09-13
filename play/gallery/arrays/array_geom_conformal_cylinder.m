% Conformal cylinder
% Sector-switched aperture on a cylindrical fairing

Nc = 32; Rc = 1.0; sector = 120;
th = -90:1:90;
AFc = zeros(1, numel(th));
Nel = 0;                        % elements inside the active sector
for n = 1:Nc
    an = -180 + 360*(n-1)/Nc;
    if abs(an) <= sector/2
        AFc = AFc + exp(1i*2*pi*Rc*(cosd(an).*cosd(th) + sind(an).*sind(th)));
        Nel = Nel + 1;
    end
end
pat = abs(AFc) / max(abs(AFc));  % normalized beam pattern (peak 1):
                                  % ring phases never align perfectly, so
                                  % normalize by the actual maximum
figure;
polarplot(th*pi/180, pat);
title('Cylinder conformal, sector');

%{
@play
{
 "title": "Conformal cylinder",
 "groups": [
  {
   "name": "Geometry",
   "controls": [
    {
     "var": "Nc",
     "label": "Elements",
     "min": 8,
     "max": 64,
     "step": 4,
     "default": 32
    },
    {
     "var": "sector",
     "label": "Sector °",
     "min": 30,
     "max": 360,
     "step": 30,
     "default": 120
    }
   ]
  }
 ]
}
%}
