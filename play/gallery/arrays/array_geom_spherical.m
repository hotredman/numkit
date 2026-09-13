% Spherical array
% Isotropic hemispherical coverage

Nrings = 5;
th = 0:2:180;
AFs = zeros(1, numel(th));
for ring = 0:Nrings-1
    elr = ring * 30;
    cnt = max(4, round(12*cosd(elr)));
    for m = 1:cnt
        az = 360*(m-1)/cnt;
        for it = 1:numel(th)
            u = cosd(th(it))*cosd(az) + sind(th(it))*sind(az);
            AFs(it) = AFs(it) + exp(1i*pi*u*sind(elr));
        end
    end
end
pat = abs(AFs) / max(abs(AFs));     % normalized (peak 1)
figure;
plot(th, pat);
title('Spherical cap coverage'); xlabel('\theta deg'); grid on;

%{
@play
{
 "title": "Spherical array",
 "groups": [
  {
   "name": "Geometry",
   "controls": [
    {
     "var": "Nrings",
     "label": "Ring count",
     "min": 2,
     "max": 8,
     "step": 1,
     "default": 5
    }
   ]
  }
 ]
}
%}
