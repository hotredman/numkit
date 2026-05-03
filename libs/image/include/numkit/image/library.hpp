// libs/image/include/numkit/image/library.hpp
//
// Image Processing Toolbox builtins. Mirrors MATLAB's documentation
// root `/help/images/`. Function-form only — no OOP class hierarchy
// (blockedImage, affinetform2d, ROI shapes, etc. intentionally absent
// per project decision to remain a "super-calculator").
//
// Sub-areas (each its own subdirectory under src/):
//   arithmetic/        — imadd, imsubtract, immultiply, imdivide,
//                        imabsdiff, imcomplement, imlincomb,
//                        imapplymatrix
//   type_convert/      — im2double / im2single / im2uint8 / mat2gray ...
//   color/             — rgb2gray / rgb2hsv / rgb2lab / xyz / ycbcr ...
//   filter/            — imfilter / fspecial / imgaussfilt / medfilt2 ...
//   morph/             — strel / imerode / imdilate / bwlabel ...
//
// Each function is registered as `image.<sub>.<name>` AND aliased into
// the flat `compat.<name>` namespace so MATLAB-style scripts can call
// it directly after `import compat.*`.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class ImageLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
