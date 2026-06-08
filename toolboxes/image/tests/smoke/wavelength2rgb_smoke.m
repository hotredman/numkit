clear

import compat.*

% wavelength2rgb — visible-spectrum wavelength → RGB.

fprintf('--- scalar 400 nm ---\n');
disp(wavelength2rgb(400));
fprintf('  expect: [0.51222 0 0.70849]\n\n');

fprintf('--- scalar 500 nm (cyan-ish) ---\n');
disp(wavelength2rgb(500));

fprintf('--- vector [400 410] ---\n');
disp(wavelength2rgb([400 410]));
fprintf('  expect: 1×2×3, [0.51222 0; 0; 0.70849; 0.49242 0; 0; 0.85736]\n\n');

fprintf('--- uint8 cast ---\n');
disp(wavelength2rgb(400, 'uint8'));
fprintf('  expect: [131 0 181]\n');

fprintf('\n--- out-of-band wavelengths → black ---\n');
disp(wavelength2rgb(300));
disp(wavelength2rgb(900));
