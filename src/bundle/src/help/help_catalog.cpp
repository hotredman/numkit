#include <numkit/bundle/help/help_catalog.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace numkit::bundle {

namespace {
std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string padRight(const std::string &s, size_t width) {
    if (s.length() >= width) return s + " ";
    return s + std::string(width - s.length(), ' ');
}
} // namespace

const HelpCatalog &HelpCatalog::instance() {
    static HelpCatalog cat;
    return cat;
}

HelpCatalog::HelpCatalog() {
    initCategories();
}

const HelpCategory *HelpCatalog::findCategory(std::string name) const {
    name = toLowerStr(name);
    if (name == "images" || name == "image_processing") name = "image";
    if (name == "signals" || name == "signal_processing") name = "signal";
    if (name == "optimization") name = "optim";
    if (name == "statistics") name = "stats";
    if (name == "linalg") name = "matfun";
    if (name == "matrix") name = "elmat";
    if (name == "math") name = "elfun";
    if (name == "strings") name = "strfun";
    if (name == "time" || name == "dates") name = "timefun";
    if (name == "io" || name == "file") name = "iofun";
    if (name == "plots" || name == "plot") name = "graphics";

    auto it = categoryIndex_.find(name);
    if (it != categoryIndex_.end() && it->second < categories_.size()) {
        return &categories_[it->second];
    }
    return nullptr;
}

const HelpEntry *HelpCatalog::findFunction(std::string name) const {
    name = toLowerStr(name);
    auto it = functionIndex_.find(name);
    if (it != functionIndex_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string HelpCatalog::formatAllCategories() const {
    std::ostringstream os;
    os << "Numkit Help Topics:\n\n";
    os << "  Standard MATLAB Library:\n";
    for (const auto &cat : categories_) {
        os << "    " << padRight(cat.name, 14) << "- " << cat.title << "\n";
    }
    os << "\nType \"help <topic>\" for a list of functions in that topic.\n";
    os << "Type \"help <function>\" for documentation on a specific function.\n";
    return os.str();
}

std::string HelpCatalog::formatCategory(const std::string &catName) const {
    const HelpCategory *cat = findCategory(catName);
    if (!cat) {
        return "Topic '" + catName + "' not found. Type 'help' for a list of available topics.\n";
    }

    std::ostringstream os;
    os << "  " << cat->title << "\n\n";
    for (const auto &sec : cat->sections) {
        os << "  " << sec.title << "\n";
        for (const auto &entry : sec.entries) {
            os << "    " << padRight(entry.name, 14) << "- " << entry.summary << "\n";
        }
        os << "\n";
    }
    return os.str();
}

std::string HelpCatalog::formatFunction(const std::string &funcName) const {
    const HelpEntry *entry = findFunction(funcName);
    if (!entry) {
        return "'" + funcName + "' not found. Type 'help' for a list of topics.\n";
    }

    std::ostringstream os;
    std::string upperName = entry->name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    os << " " << upperName << "    " << entry->summary << "\n\n";
    if (!entry->signature.empty()) {
        os << "    " << entry->signature << "\n\n";
    }
    if (!entry->doc.empty()) {
        os << "    " << entry->doc << "\n";
    } else {
        os << "    " << upperName << "(...) computes the " << entry->summary << "\n";
    }
    return os.str();
}

std::vector<std::string> HelpCatalog::getCategoryFunctions(const std::string &catName) const {
    std::vector<std::string> list;
    const HelpCategory *cat = findCategory(catName);
    if (cat) {
        for (const auto &sec : cat->sections) {
            for (const auto &entry : sec.entries) {
                list.push_back(entry.name);
            }
        }
    }
    std::sort(list.begin(), list.end());
    return list;
}

std::vector<std::string> HelpCatalog::getAllFunctions() const {
    std::vector<std::string> list;
    list.reserve(functionIndex_.size());
    for (const auto &[name, _] : functionIndex_) {
        list.push_back(name);
    }
    std::sort(list.begin(), list.end());
    return list;
}

void HelpCatalog::initCategories() {
    categories_ = {
        // ── 1. elmat: Elementary matrices and matrix manipulation ────────
        {
            "elmat",
            "Elementary matrices and matrix manipulation.",
            {
                {
                    "Elementary matrices.",
                    {
                        {"zeros", "Zeros array.", "zeros(N) or zeros(M, N)", "zeros(M,N) returns an M-by-N matrix of zeros."},
                        {"ones", "Ones array.", "ones(N) or ones(M, N)", "ones(M,N) returns an M-by-N matrix of ones."},
                        {"eye", "Identity matrix.", "eye(N) or eye(M, N)", "eye(N) returns an N-by-N identity matrix."},
                        {"rand", "Uniformly distributed random numbers.", "rand(M, N)", "rand(M,N) returns an M-by-N matrix of random numbers in (0, 1)."},
                        {"randn", "Normally distributed random numbers.", "randn(M, N)", "randn(M,N) returns an M-by-N matrix of standard normal random numbers."},
                        {"randi", "Pseudorandom integers from a uniform discrete distribution.", "randi(IMAX, M, N)", "randi(IMAX, M, N) returns random integers between 1 and IMAX."},
                        {"linspace", "Linearly spaced vector.", "linspace(X1, X2, N)", "linspace(X1, X2, N) generates N linearly spaced points between X1 and X2."},
                        {"logspace", "Logarithmically spaced vector.", "logspace(D1, D2, N)", "logspace(D1, D2, N) generates N logarithmically spaced points between 10^D1 and 10^D2."},
                        {"freqspace", "Frequency spacing for frequency response.", "freqspace(N)", "freqspace returns the frequency range for frequency response."},
                        {"meshgrid", "2-D and 3-D grids for plotting.", "[X, Y] = meshgrid(x, y)", "meshgrid transforms domain vectors x and y into 2-D grid arrays X and Y."},
                        {"ndgrid", "Rectangular grid in N-D space.", "[X1, X2] = ndgrid(x1, x2)", "ndgrid generates N-D coordinate grids."}
                    }
                },
                {
                    "Basic array information.",
                    {
                        {"size", "Size of array.", "D = size(X)", "size(X) returns the dimensions of array X."},
                        {"length", "Length of vector.", "L = length(X)", "length(X) returns the size of the longest dimension of X."},
                        {"ndims", "Number of array dimensions.", "N = ndims(X)", "ndims(X) returns the number of dimensions in array X."},
                        {"numel", "Number of elements in array.", "N = numel(X)", "numel(X) returns the total number of elements in X."},
                        {"isempty", "True for empty array.", "TF = isempty(X)", "isempty(X) returns true if X has any zero dimension."},
                        {"isequal", "True if arrays are numerically equal.", "TF = isequal(A, B)", "isequal(A, B) returns true if A and B have identical size and values."},
                        {"isequaln", "True if arrays are equal, treating NaNs as equal.", "TF = isequaln(A, B)", "isequaln treats NaN elements as equal."}
                    }
                },
                {
                    "Matrix manipulation.",
                    {
                        {"cat", "Concatenate arrays.", "C = cat(DIM, A, B, ...)", "cat concatenates arrays along the specified dimension DIM."},
                        {"horzcat", "Horizontal concatenation [A, B].", "C = [A, B] or horzcat(A, B)", "horzcat concatenates matrices horizontally."},
                        {"vertcat", "Vertical concatenation [A; B].", "C = [A; B] or vertcat(A, B)", "vertcat concatenates matrices vertically."},
                        {"reshape", "Reshape array.", "B = reshape(A, M, N)", "reshape(A, M, N) rearranges elements of A into an M-by-N matrix."},
                        {"diag", "Diagonal matrices and diagonals of matrix.", "D = diag(V, K)", "diag(V) extracts the diagonal or constructs a diagonal matrix."},
                        {"blkdiag", "Block diagonal concatenation.", "B = blkdiag(A, B, C)", "blkdiag constructs a block diagonal matrix from input matrices."},
                        {"tril", "Extract lower triangular part.", "L = tril(X, K)", "tril(X) returns the lower triangular part of matrix X."},
                        {"triu", "Extract upper triangular part.", "U = triu(X, K)", "triu(X) returns the upper triangular part of matrix X."},
                        {"fliplr", "Flip matrix in left/right direction.", "B = fliplr(A)", "fliplr flips matrix columns horizontally."},
                        {"flipud", "Flip matrix in up/down direction.", "B = flipud(A)", "flipud flips matrix rows vertically."},
                        {"flip", "Flip order of elements.", "B = flip(A, DIM)", "flip reverses elements along dimension DIM."},
                        {"rot90", "Rotate matrix 90 degrees.", "B = rot90(A, K)", "rot90 rotates matrix A by K*90 degrees counterclockwise."},
                        {"repmat", "Replicate and tile array.", "B = repmat(A, M, N)", "repmat tiles matrix A into an M-by-N block pattern."},
                        {"repelem", "Replicate elements of array.", "B = repelem(A, R, C)", "repelem replicates each element of A."},
                        {"permute", "Permute array dimensions.", "B = permute(A, ORDER)", "permute rearranges dimensions of N-D array A."},
                        {"ipermute", "Inverse permute array dimensions.", "A = ipermute(B, ORDER)", "ipermute inverts the effect of permute."},
                        {"shiftdim", "Shift array dimensions.", "B = shiftdim(A, N)", "shiftdim shifts the dimensions of A by N steps."},
                        {"circshift", "Shift array circularly.", "B = circshift(A, K)", "circshift circularly shifts array elements by K."},
                        {"squeeze", "Remove singleton dimensions.", "B = squeeze(A)", "squeeze removes dimensions of size 1 from N-D array A."},
                        {"find", "Find indices and values of non-zero elements.", "[row, col, v] = find(X)", "find returns the linear or subscript indices of non-zero elements."},
                        {"sub2ind", "Linear index from multiple subscripts.", "IND = sub2ind(SZ, I, J)", "sub2ind converts row and column subscripts to linear index."},
                        {"ind2sub", "Multiple subscripts from linear index.", "[I, J] = ind2sub(SZ, IND)", "ind2sub converts linear index to row and column subscripts."},
                        {"bsxfun", "Binary singleton expansion function.", "C = bsxfun(FUN, A, B)", "bsxfun applies element-wise binary operation with singleton expansion."}
                    }
                },
                {
                    "Constants and type tests.",
                    {
                        {"eps", "Floating-point relative accuracy.", "E = eps(X)", "eps returns the distance from 1.0 to the next larger double precision number."},
                        {"pi", "Ratio of circle circumference to diameter.", "P = pi", "pi returns the floating-point value of 3.141592653589793."},
                        {"inf", "Infinity.", "inf", "inf represents IEEE infinity."},
                        {"nan", "Not-a-Number.", "nan", "nan represents IEEE Not-a-Number."},
                        {"isnan", "True for Not-a-Number elements.", "TF = isnan(X)", "isnan(X) returns logical array with true for NaN elements."},
                        {"isinf", "True for infinite elements.", "TF = isinf(X)", "isinf(X) returns logical array with true for +/- Inf elements."},
                        {"isfinite", "True for finite elements.", "TF = isfinite(X)", "isfinite(X) returns true for elements that are neither NaN nor Inf."},
                        {"isscalar", "True for scalar variable.", "TF = isscalar(X)", "isscalar(X) returns true if numel(X) == 1."},
                        {"isvector", "True for vector variable.", "TF = isvector(X)", "isvector(X) returns true for 1-D arrays."},
                        {"isrow", "True for row vector.", "TF = isrow(X)", "isrow(X) returns true for 1-by-N matrices."},
                        {"iscolumn", "True for column vector.", "TF = iscolumn(X)", "iscolumn(X) returns true for N-by-1 matrices."},
                        {"ismatrix", "True for 2-D matrix.", "TF = ismatrix(X)", "ismatrix(X) returns true for 2-D arrays."},
                        {"true", "True logical array.", "true(M, N)", "true(M, N) returns an M-by-N array of logical ones."},
                        {"false", "False logical array.", "false(M, N)", "false(M, N) returns an M-by-N array of logical zeros."}
                    }
                }
            }
        },

        // ── 2. elfun: Elementary math functions ──────────────────────────
        {
            "elfun",
            "Elementary math functions.",
            {
                {
                    "Trigonometric.",
                    {
                        {"sin", "Sine (in radians).", "Y = sin(X)", "sin(X) computes the sine of the elements of X in radians."},
                        {"sind", "Sine of argument in degrees.", "Y = sind(X)", "sind(X) computes the sine in degrees."},
                        {"sinh", "Hyperbolic sine.", "Y = sinh(X)", "sinh(X) computes the hyperbolic sine."},
                        {"asin", "Inverse sine.", "Y = asin(X)", "asin(X) computes the arcsine in radians."},
                        {"asind", "Inverse sine in degrees.", "Y = asind(X)", "asind(X) computes the arcsine in degrees."},
                        {"asinh", "Inverse hyperbolic sine.", "Y = asinh(X)", "asinh(X) computes the inverse hyperbolic sine."},
                        {"cos", "Cosine (in radians).", "Y = cos(X)", "cos(X) computes the cosine of the elements of X in radians."},
                        {"cosd", "Cosine of argument in degrees.", "Y = cosd(X)", "cosd(X) computes the cosine in degrees."},
                        {"cosh", "Hyperbolic cosine.", "Y = cosh(X)", "cosh(X) computes the hyperbolic cosine."},
                        {"acos", "Inverse cosine.", "Y = acos(X)", "acos(X) computes the arccosine in radians."},
                        {"acosd", "Inverse cosine in degrees.", "Y = acosd(X)", "acosd(X) computes the arccosine in degrees."},
                        {"acosh", "Inverse hyperbolic cosine.", "Y = acosh(X)", "acosh(X) computes the inverse hyperbolic cosine."},
                        {"tan", "Tangent (in radians).", "Y = tan(X)", "tan(X) computes the tangent of the elements of X in radians."},
                        {"tand", "Tangent of argument in degrees.", "Y = tand(X)", "tand(X) computes the tangent in degrees."},
                        {"tanh", "Hyperbolic tangent.", "Y = tanh(X)", "tanh(X) computes the hyperbolic tangent."},
                        {"atan", "Inverse tangent.", "Y = atan(X)", "atan(X) computes the arctangent in radians."},
                        {"atand", "Inverse tangent in degrees.", "Y = atand(X)", "atand(X) computes the arctangent in degrees."},
                        {"atan2", "Four-quadrant inverse tangent.", "Y = atan2(Y, X)", "atan2(Y, X) computes the four-quadrant arctangent in [-pi, pi]."},
                        {"atan2d", "Four-quadrant inverse tangent in degrees.", "Y = atan2d(Y, X)", "atan2d computes the four-quadrant arctangent in degrees."},
                        {"atanh", "Inverse hyperbolic tangent.", "Y = atanh(X)", "atanh(X) computes the inverse hyperbolic tangent."},
                        {"sec", "Secant.", "Y = sec(X)", "sec(X) computes the secant 1/cos(X)."},
                        {"csc", "Cosecant.", "Y = csc(X)", "csc(X) computes the cosecant 1/sin(X)."},
                        {"cot", "Cotangent.", "Y = cot(X)", "cot(X) computes the cotangent 1/tan(X)."},
                        {"hypot", "Square root of sum of squares.", "H = hypot(A, B)", "hypot(A, B) computes sqrt(A.^2 + B.^2) avoiding underflow/overflow."},
                        {"deg2rad", "Convert degrees to radians.", "R = deg2rad(D)", "deg2rad converts angle values from degrees to radians."},
                        {"rad2deg", "Convert radians to degrees.", "D = rad2deg(R)", "rad2deg converts angle values from radians to degrees."}
                    }
                },
                {
                    "Exponential and logarithmic.",
                    {
                        {"exp", "Exponential e^x.", "Y = exp(X)", "exp(X) computes the exponential e^x for each element of X."},
                        {"log", "Natural logarithm ln(x).", "Y = log(X)", "log(X) computes the natural logarithm base e."},
                        {"log10", "Common (base 10) logarithm.", "Y = log10(X)", "log10(X) computes the logarithm base 10."},
                        {"log2", "Base 2 logarithm and dissect floating-point number.", "Y = log2(X)", "log2(X) computes the logarithm base 2."},
                        {"pow2", "Base 2 power and scale.", "Y = pow2(X)", "pow2(X) computes 2.^X."},
                        {"sqrt", "Square root.", "Y = sqrt(X)", "sqrt(X) computes the square root of elements of X."},
                        {"cbrt", "Cube root.", "Y = cbrt(X)", "cbrt(X) computes the real cube root."},
                        {"nextpow2", "Next higher power of 2.", "P = nextpow2(N)", "nextpow2(N) returns the smallest integer P such that 2^P >= abs(N)."}
                    }
                },
                {
                    "Complex numbers.",
                    {
                        {"abs", "Absolute value and complex magnitude.", "Y = abs(X)", "abs(X) computes the absolute value or complex magnitude |X|."},
                        {"angle", "Phase angle in radians.", "P = angle(Z)", "angle(Z) computes the phase angle in radians for complex Z."},
                        {"complex", "Construct complex data from real and imaginary parts.", "Z = complex(A, B)", "complex(A, B) creates complex array A + B*i."},
                        {"conj", "Complex conjugate.", "Y = conj(Z)", "conj(Z) computes the complex conjugate of Z."},
                        {"real", "Real part of complex number.", "X = real(Z)", "real(Z) returns the real part of complex array Z."},
                        {"imag", "Imaginary part of complex number.", "Y = imag(Z)", "imag(Z) returns the imaginary part of complex array Z."}
                    }
                },
                {
                    "Rounding and remainder.",
                    {
                        {"round", "Round to nearest integer.", "Y = round(X)", "round(X) rounds elements of X to the nearest integers."},
                        {"floor", "Round towards minus infinity.", "Y = floor(X)", "floor(X) rounds elements down to nearest integers."},
                        {"ceil", "Round towards plus infinity.", "Y = ceil(X)", "ceil(X) rounds elements up to nearest integers."},
                        {"fix", "Round towards zero.", "Y = fix(X)", "fix(X) truncates decimals towards zero."},
                        {"mod", "Modulus (signed remainder after division).", "M = mod(X, Y)", "mod(X, Y) computes X - Y.*floor(X./Y)."},
                        {"rem", "Remainder after division.", "R = rem(X, Y)", "rem(X, Y) computes X - Y.*fix(X./Y)."},
                        {"sign", "Signum function.", "S = sign(X)", "sign(X) returns 1 for X>0, -1 for X<0, and 0 for X==0."}
                    }
                }
            }
        },

        // ── 3. matfun: Matrix algebra and linear equations ───────────────
        {
            "matfun",
            "Matrix functions - numerical linear algebra.",
            {
                {
                    "Matrix analysis and norms.",
                    {
                        {"norm", "Matrix or vector norm.", "N = norm(A, p)", "norm computes the vector or matrix norm."},
                        {"rank", "Matrix rank.", "K = rank(A)", "rank(A) computes the number of linearly independent rows or columns."},
                        {"det", "Matrix determinant.", "D = det(A)", "det(A) computes the determinant of square matrix A."},
                        {"trace", "Sum of diagonal elements.", "T = trace(A)", "trace(A) returns the sum of diagonal elements of A."},
                        {"null", "Null space.", "Z = null(A)", "null(A) computes an orthonormal basis for the null space of A."},
                        {"orth", "Orthogonalization.", "Q = orth(A)", "orth(A) computes an orthonormal basis for the range of A."},
                        {"rref", "Reduced row echelon form.", "[R, jb] = rref(A)", "rref(A) produces the reduced row echelon form of A."},
                        {"cond", "Condition number with respect to inversion.", "C = cond(A)", "cond(A) returns the 2-norm condition number of matrix A."},
                        {"rcond", "Reciprocal condition estimator.", "C = rcond(A)", "rcond(A) estimates the reciprocal 1-norm condition number."}
                    }
                },
                {
                    "Linear equations and factorizations.",
                    {
                        {"inv", "Matrix inverse.", "Y = inv(A)", "inv(A) computes the inverse of square matrix A."},
                        {"pinv", "Pseudoinverse.", "Y = pinv(A)", "pinv(A) computes the Moore-Penrose pseudoinverse."},
                        {"linsolve", "Solve linear system of equations.", "X = linsolve(A, B)", "linsolve(A, B) solves linear system A*X = B with options."},
                        {"decomposition", "Matrix decomposition object.", "d = decomposition(A)", "decomposition creates reusable factorization objects."},
                        {"lu", "LU factorization with partial pivoting.", "[L, U, P] = lu(A)", "lu(A) computes lower and upper triangular factors such that P*A = L*U."},
                        {"qr", "Orthogonal-triangular decomposition.", "[Q, R] = qr(A)", "qr(A) factorizes matrix A into orthogonal Q and upper triangular R."},
                        {"chol", "Cholesky factorization.", "R = chol(A)", "chol(A) computes upper triangular R such that R'*R = A for positive definite A."},
                        {"svd", "Singular value decomposition.", "[U, S, V] = svd(A)", "svd(A) factorizes matrix A = U*S*V' into singular vectors and values."},
                        {"eig", "Eigenvalues and eigenvectors.", "[V, D] = eig(A)", "eig(A) computes eigenvalues and eigenvectors of square matrix A."},
                        {"schur", "Schur decomposition.", "[U, T] = schur(A)", "schur(A) produces unitary U and Schur matrix T such that A = U*T*U'."},
                        {"balance", "Diagonal scaling for eigenvalue accuracy.", "[T, B] = balance(A)", "balance balances square matrix A to improve eigenvalue condition."}
                    }
                },
                {
                    "Matrix functions.",
                    {
                        {"expm", "Matrix exponential.", "E = expm(A)", "expm(A) computes the matrix exponential e^A via Pade approximation."},
                        {"logm", "Matrix logarithm.", "L = logm(A)", "logm(A) computes the principal matrix logarithm."},
                        {"sqrtm", "Matrix square root.", "X = sqrtm(A)", "sqrtm(A) computes matrix square root X such that X*X = A."},
                        {"funm", "Evaluate general matrix function.", "F = funm(A, @fun)", "funm evaluates an analytic scalar function on a square matrix."}
                    }
                }
            }
        },

        // ── 4. datafun: Data analysis and Fourier transforms ─────────────
        {
            "datafun",
            "Data analysis, summary statistics and Fourier transforms.",
            {
                {
                    "Summary statistics.",
                    {
                        {"sum", "Sum of elements.", "S = sum(X, DIM)", "sum(X) computes the sum of array elements along dimension DIM."},
                        {"prod", "Product of elements.", "P = prod(X, DIM)", "prod(X) computes product of elements along dimension DIM."},
                        {"cumsum", "Cumulative sum.", "Y = cumsum(X)", "cumsum(X) computes cumulative sum along dimension."},
                        {"cumprod", "Cumulative product.", "Y = cumprod(X)", "cumprod(X) computes cumulative product along dimension."},
                        {"diff", "Differences and approximate derivatives.", "Y = diff(X)", "diff(X) calculates differences between adjacent elements."},
                        {"gradient", "Numerical gradient.", "[FX, FY] = gradient(F)", "gradient(F) computes numerical gradient of N-D array."},
                        {"mean", "Average or mean value.", "M = mean(X, DIM)", "mean(X) calculates arithmetic mean along dimension DIM."},
                        {"median", "Median value.", "M = median(X, DIM)", "median(X) calculates median along dimension DIM."},
                        {"mode", "Most frequent value.", "M = mode(X)", "mode(X) returns the sample mode of elements."},
                        {"std", "Standard deviation.", "S = std(X)", "std(X) computes sample standard deviation."},
                        {"var", "Variance.", "V = var(X)", "var(X) computes sample variance."},
                        {"min", "Minimum value.", "[M, I] = min(X)", "min(X) returns the minimum values along dimension."},
                        {"max", "Maximum value.", "[M, I] = max(X)", "max(X) returns the maximum values along dimension."},
                        {"bounds", "Smallest and largest elements.", "[L, U] = bounds(X)", "bounds(X) returns [min(X), max(X)]."},
                        {"sort", "Sort array elements.", "[B, I] = sort(A, DIM, MODE)", "sort sorts array elements in ascending or descending order."},
                        {"sortrows", "Sort matrix rows.", "[B, I] = sortrows(A, COLS)", "sortrows sorts rows of matrix A based on specified columns."}
                    }
                },
                {
                    "Correlation and convolution.",
                    {
                        {"cov", "Covariance matrix.", "C = cov(X)", "cov(X) computes covariance matrix."},
                        {"corrcoef", "Correlation coefficients.", "R = corrcoef(X)", "corrcoef(X) computes matrix of correlation coefficients."},
                        {"conv", "1-D Convolution and polynomial multiplication.", "C = conv(A, B, SHAPE)", "conv convolves vectors A and B."},
                        {"conv2", "2-D Convolution.", "C = conv2(A, B, SHAPE)", "conv2 computes 2-D convolution of matrices A and B."},
                        {"filter", "1-D digital filter.", "Y = filter(B, A, X)", "filter filters input vector X with rational transfer function B/A."},
                        {"filter2", "2-D digital filter.", "Y = filter2(H, X, SHAPE)", "filter2 applies 2-D FIR filter H to matrix X."}
                    }
                },
                {
                    "Fourier transforms.",
                    {
                        {"fft", "1-D Fast Fourier Transform.", "Y = fft(X, N, DIM)", "fft(X) computes the discrete Fourier transform via FFT algorithm."},
                        {"ifft", "1-D Inverse Fast Fourier Transform.", "Y = ifft(X, N, DIM)", "ifft(X) computes inverse discrete Fourier transform."},
                        {"fft2", "2-D Fast Fourier Transform.", "Y = fft2(X)", "fft2(X) computes 2-D discrete Fourier transform."},
                        {"ifft2", "2-D Inverse Fast Fourier Transform.", "Y = ifft2(X)", "ifft2(X) computes 2-D inverse discrete Fourier transform."},
                        {"fftn", "N-D Fast Fourier Transform.", "Y = fftn(X)", "fftn(X) computes N-D discrete Fourier transform."},
                        {"ifftn", "N-D Inverse Fast Fourier Transform.", "Y = ifftn(X)", "ifftn(X) computes N-D inverse discrete Fourier transform."},
                        {"fftshift", "Shift zero-frequency component to center.", "Y = fftshift(X)", "fftshift centers zero frequency in Fourier transform."},
                        {"ifftshift", "Inverse zero-frequency shift.", "Y = ifftshift(X)", "ifftshift undoes the effect of fftshift."}
                    }
                },
                {
                    "Moving window statistics.",
                    {
                        {"movmean", "Moving average.", "Y = movmean(A, K)", "movmean computes moving average over window length K."},
                        {"movmedian", "Moving median.", "Y = movmedian(A, K)", "movmedian computes moving median over window length K."},
                        {"movstd", "Moving standard deviation.", "Y = movstd(A, K)", "movstd computes moving standard deviation."},
                        {"movvar", "Moving variance.", "Y = movvar(A, K)", "movvar computes moving variance."},
                        {"movmin", "Moving minimum.", "Y = movmin(A, K)", "movmin computes moving minimum."},
                        {"movmax", "Moving maximum.", "Y = movmax(A, K)", "movmax computes moving maximum."},
                        {"movsum", "Moving sum.", "Y = movsum(A, K)", "movsum computes moving sum."}
                    }
                }
            }
        },

        // ── 5. specfun: Special mathematical functions ───────────────────
        {
            "specfun",
            "Special mathematical functions.",
            {
                {
                    "Gamma, beta and factorial.",
                    {
                        {"gamma", "Gamma function.", "Y = gamma(X)", "gamma(X) computes the gamma function."},
                        {"gammainc", "Incomplete gamma function.", "Y = gammainc(X, A)", "gammainc computes regularized lower incomplete gamma function."},
                        {"gammaln", "Logarithm of gamma function.", "Y = gammaln(X)", "gammaln(X) computes ln(gamma(X)) accurately."},
                        {"psi", "Digamma and polygamma functions.", "Y = psi(K, X)", "psi(X) computes the logarithmic derivative of gamma function."},
                        {"beta", "Beta function.", "Y = beta(Z, W)", "beta(Z, W) computes the beta function B(Z, W)."},
                        {"betainc", "Incomplete beta function.", "Y = betainc(X, Z, W)", "betainc computes regularized incomplete beta function."},
                        {"betaln", "Logarithm of beta function.", "Y = betaln(Z, W)", "betaln(Z, W) computes ln(beta(Z, W))."},
                        {"factorial", "Factorial function.", "Y = factorial(N)", "factorial(N) computes N!."}
                    }
                },
                {
                    "Error and Bessel functions.",
                    {
                        {"erf", "Error function.", "Y = erf(X)", "erf(X) computes the error function 2/sqrt(pi)*int_0^X e^(-t^2) dt."},
                        {"erfc", "Complementary error function.", "Y = erfc(X)", "erfc(X) computes 1 - erf(X)."},
                        {"erfinv", "Inverse error function.", "Y = erfinv(X)", "erfinv(X) satisfies erf(erfinv(X)) = X."},
                        {"erfcinv", "Inverse complementary error function.", "Y = erfcinv(X)", "erfcinv(X) satisfies erfc(erfcinv(X)) = X."},
                        {"besselj", "Bessel function of the first kind.", "J = besselj(NU, Z)", "besselj computes Bessel function J_nu(z)."},
                        {"bessely", "Bessel function of the second kind.", "Y = bessely(NU, Z)", "bessely computes Bessel function Y_nu(z)."},
                        {"besseli", "Modified Bessel function of first kind.", "I = besseli(NU, Z)", "besseli computes modified Bessel I_nu(z)."},
                        {"besselk", "Modified Bessel function of second kind.", "K = besselk(NU, Z)", "besselk computes modified Bessel K_nu(z)."},
                        {"legendre", "Associated Legendre functions.", "P = legendre(N, X)", "legendre computes associated Legendre functions P_n^m(x)."},
                        {"ellipke", "Complete elliptic integrals of first and second kind.", "[K, E] = ellipke(M)", "ellipke computes complete elliptic integrals."}
                    }
                }
            }
        },

        // ── 6. polyfun: Polynomials, interpolation and numerical calculus ───
        {
            "polyfun",
            "Polynomials, interpolation and numerical integration.",
            {
                {
                    "Polynomials.",
                    {
                        {"roots", "Polynomial roots.", "R = roots(P)", "roots(P) computes the roots of polynomial with coefficients P."},
                        {"poly", "Polynomial with specified roots or characteristic polynomial.", "P = poly(R)", "poly(R) computes polynomial coefficients from roots."},
                        {"polyval", "Evaluate polynomial.", "Y = polyval(P, X)", "polyval(P, X) evaluates polynomial P at points X."},
                        {"polyfit", "Polynomial curve fitting.", "P = polyfit(X, Y, N)", "polyfit(X, Y, N) fits degree N polynomial in least-squares sense."},
                        {"polyder", "Polynomial derivative.", "Q = polyder(P)", "polyder calculates derivative of polynomial."},
                        {"polyint", "Polynomial integration.", "Q = polyint(P, K)", "polyint integrates polynomial with constant K."},
                        {"deconv", "Deconvolution and polynomial division.", "[Q, R] = deconv(B, A)", "deconv deconvolves vector A from vector B."}
                    }
                },
                {
                    "Interpolation.",
                    {
                        {"interp1", "1-D data interpolation.", "Vq = interp1(X, V, Xq, METHOD)", "interp1 interpolates 1-D data (linear, cubic, spline, nearest)."},
                        {"interp2", "2-D data interpolation.", "Vq = interp2(X, Y, V, Xq, Yq, METHOD)", "interp2 interpolates 2-D gridded data."},
                        {"interp3", "3-D data interpolation.", "Vq = interp3(X, Y, Z, V, Xq, Yq, Zq)", "interp3 interpolates 3-D volumetric data."},
                        {"interpn", "N-D data interpolation.", "Vq = interpn(X1, X2, ..., V, Xq1, Xq2, ...)", "interpn interpolates N-D data on regular grids."},
                        {"spline", "Cubic spline data interpolation.", "Yq = spline(X, Y, Xq)", "spline computes cubic spline interpolation with not-a-knot end conditions."},
                        {"pchip", "Piecewise Cubic Hermite Interpolating Polynomial.", "Yq = pchip(X, Y, Xq)", "pchip computes shape-preserving Hermite interpolation."}
                    }
                },
                {
                    "Numerical integration.",
                    {
                        {"integral", "Numerically evaluate integral.", "Q = integral(FUN, A, B)", "integral computes adaptive numerical quadrature of scalar function."},
                        {"integral2", "2-D numerical integration.", "Q = integral2(FUN, XMIN, XMAX, YMIN, YMAX)", "integral2 evaluates double integrals over rectangular domains."},
                        {"integral3", "3-D numerical integration.", "Q = integral3(FUN, XMIN, XMAX, YMIN, YMAX, ZMIN, ZMAX)", "integral3 evaluates triple integrals."},
                        {"trapz", "Trapezoidal numerical integration.", "Z = trapz(X, Y)", "trapz computes trapezoidal integration over discrete points."},
                        {"cumtrapz", "Cumulative trapezoidal integration.", "Z = cumtrapz(X, Y)", "cumtrapz computes cumulative trapezoidal integral."}
                    }
                }
            }
        },

        // ── 7. strfun: Character and string manipulation ─────────────────
        {
            "strfun",
            "Character and string manipulation.",
            {
                {
                    "String comparison and search.",
                    {
                        {"strcmp", "Compare strings.", "TF = strcmp(S1, S2)", "strcmp returns true if strings are identical."},
                        {"strncmp", "Compare first N characters of strings.", "TF = strncmp(S1, S2, N)", "strncmp compares initial N characters."},
                        {"strcmpi", "Compare strings ignoring case.", "TF = strcmpi(S1, S2)", "strcmpi performs case-insensitive comparison."},
                        {"strncmpi", "Compare first N characters ignoring case.", "TF = strncmpi(S1, S2, N)", "strncmpi compares first N characters case-insensitively."},
                        {"strfind", "Find one string within another.", "K = strfind(STR, PAT)", "strfind returns starting indices of pattern in string."},
                        {"strrep", "Find and replace substring.", "S = strrep(STR, OLD, NEW)", "strrep replaces occurrences of OLD with NEW."},
                        {"contains", "Determine if pattern is in string.", "TF = contains(STR, PAT)", "contains returns true if pattern is found."},
                        {"startsWith", "Determine if string starts with pattern.", "TF = startsWith(STR, PAT)", "startsWith returns true if string starts with pattern."},
                        {"endsWith", "Determine if string ends with pattern.", "TF = endsWith(STR, PAT)", "endsWith returns true if string ends with pattern."}
                    }
                },
                {
                    "String formatting and conversion.",
                    {
                        {"sprintf", "Format data into string.", "STR = sprintf(FORMAT, A, ...)", "sprintf formats data using C printf-style format specifiers."},
                        {"sscanf", "Read formatted data from string.", "[A, COUNT] = sscanf(STR, FORMAT)", "sscanf reads formatted data from string."},
                        {"strsplit", "Split string at delimiter.", "C = strsplit(STR, DELIM)", "strsplit splits string into cell array of substrings."},
                        {"strjoin", "Join strings with delimiter.", "S = strjoin(C, DELIM)", "strjoin joins elements of cellstr array into single string."},
                        {"strtrim", "Remove leading and trailing whitespace.", "S = strtrim(STR)", "strtrim trims whitespace from start and end."},
                        {"lower", "Convert string to lowercase.", "S = lower(STR)", "lower converts all characters to lowercase."},
                        {"upper", "Convert string to uppercase.", "S = upper(STR)", "upper converts all characters to uppercase."},
                        {"num2str", "Convert numbers to string.", "S = num2str(A, PREC)", "num2str converts numeric array to string representation."},
                        {"str2num", "Convert string to numeric matrix.", "X = str2num(STR)", "str2num parses string into numeric matrix."},
                        {"str2double", "Convert string to double precision value.", "X = str2double(STR)", "str2double converts text string to double."},
                        {"regexprep", "Replace string using regular expression.", "S = regexprep(STR, EXPR, REP)", "regexprep replaces regex matches."},
                        {"regexp", "Match regular expression.", "[START, END] = regexp(STR, EXPR)", "regexp finds matches of regular expression."},
                        {"regexpi", "Match regular expression ignoring case.", "[START, END] = regexpi(STR, EXPR)", "regexpi performs case-insensitive regex match."}
                    }
                }
            }
        },

        // ── 8. timefun: Time and dates ───────────────────────────────────
        {
            "timefun",
            "Time, dates and profiling.",
            {
                {
                    "Timing and clocks.",
                    {
                        {"tic", "Start stopwatch timer.", "tic or T = tic", "tic records current time for stopwatch measurement."},
                        {"toc", "Read stopwatch timer.", "T = toc or toc", "toc prints elapsed time since matching tic."},
                        {"clock", "Current date and time as vector.", "C = clock", "clock returns [year month day hour minute second]."},
                        {"date", "Current date string.", "D = date", "date returns formatted current date string."},
                        {"now", "Current date and time as serial date number.", "N = now", "now returns current serial date number."},
                        {"datestr", "Convert date vector or number to string.", "S = datestr(D, FORMAT)", "datestr formats date number or vector."},
                        {"datenum", "Convert date string or vector to serial date.", "N = datenum(S)", "datenum returns serial date number."},
                        {"pause", "Halt execution temporarily.", "pause(N)", "pause(N) halts execution for N seconds."},
                        {"cputime", "Total CPU time used by process.", "T = cputime", "cputime returns process elapsed CPU seconds."},
                        {"timeit", "Measure time required to run function.", "T = timeit(@fun)", "timeit measures benchmark execution time of function."}
                    }
                }
            }
        },

        // ── 9. datatypes: Data types and structures ──────────────────────
        {
            "datatypes",
            "Data types, structures, cells and object introspection.",
            {
                {
                    "Structures and cells.",
                    {
                        {"struct", "Create structure array.", "S = struct('field1', val1, ...)", "struct creates scalar or struct arrays."},
                        {"cell", "Create cell array.", "C = cell(M, N)", "cell(M, N) creates an M-by-N cell array of empty matrices."},
                        {"isstruct", "True for structures.", "TF = isstruct(S)", "isstruct(S) returns true if S is a struct."},
                        {"iscell", "True for cell array.", "TF = iscell(C)", "iscell(C) returns true if C is a cell array."},
                        {"iscellstr", "True for cell array of strings.", "TF = iscellstr(C)", "iscellstr(C) returns true if C contains only strings."},
                        {"fieldnames", "Field names of structure or class.", "NAMES = fieldnames(S)", "fieldnames returns a cellstr of field names."},
                        {"isfield", "Determine if structure has field.", "TF = isfield(S, 'name')", "isfield returns true if field exists."},
                        {"getfield", "Get structure field contents.", "V = getfield(S, 'name')", "getfield retrieves field value."},
                        {"setfield", "Set structure field contents.", "S = setfield(S, 'name', V)", "setfield assigns value to structure field."},
                        {"rmfield", "Remove fields from structure.", "S = rmfield(S, 'name')", "rmfield deletes specified field from structure."},
                        {"cellfun", "Apply function to each cell in array.", "[A, B] = cellfun(@fun, C)", "cellfun evaluates function over all elements of cell array."},
                        {"arrayfun", "Apply function to each element of array.", "B = arrayfun(@fun, A)", "arrayfun evaluates function over array elements."},
                        {"structfun", "Apply function to each field in structure.", "B = structfun(@fun, S)", "structfun applies function to each field of structure."}
                    }
                },
                {
                    "Object model and class introspection.",
                    {
                        {"class", "Class name of object.", "CN = class(OBJ)", "class(OBJ) returns the class name string."},
                        {"isa", "Determine if input is of specified class.", "TF = isa(OBJ, 'classname')", "isa tests if object belongs to class or superclass."},
                        {"isobject", "True for MATLAB OOP objects.", "TF = isobject(OBJ)", "isobject returns true for instances of classdef classes."},
                        {"isprop", "True if object declares property.", "TF = isprop(OBJ, 'propname')", "isprop tests if class has specified property."},
                        {"ismethod", "True if object declares method.", "TF = ismethod(OBJ, 'methodname')", "ismethod tests if class declares specified method."},
                        {"methods", "List class methods.", "M = methods('classname')", "methods returns a cellstr of all accessible methods."},
                        {"properties", "List class property names.", "P = properties('classname')", "properties returns a cellstr of public property names."},
                        {"containers.Map", "Key-value associative map container.", "M = containers.Map(keys, values)", "containers.Map creates key-value map collection."}
                    }
                }
            }
        },

        // ── 10. iofun: File input and output ─────────────────────────────
        {
            "iofun",
            "File input and output.",
            {
                {
                    "File open, read, write and close.",
                    {
                        {"fopen", "Open file.", "FID = fopen(FILENAME, PERMISSION)", "fopen opens file for reading or writing with specified mode."},
                        {"fclose", "Close file.", "STATUS = fclose(FID)", "fclose closes open file identifier FID (or all open files)."},
                        {"fread", "Read binary data from file.", "[A, COUNT] = fread(FID, SIZE, PRECISION)", "fread reads binary data elements from file FID."},
                        {"fwrite", "Write binary data to file.", "COUNT = fwrite(FID, A, PRECISION)", "fwrite writes binary data from A to file FID."},
                        {"fprintf", "Write formatted data to file or screen.", "fprintf(FID, FORMAT, A, ...)", "fprintf prints formatted output to file FID or stdout."},
                        {"fscanf", "Read formatted data from file.", "[A, COUNT] = fscanf(FID, FORMAT, SIZE)", "fscanf reads formatted data matching format string."},
                        {"fgetl", "Read line from file, removing newline.", "TLINE = fgetl(FID)", "fgetl reads next line from file without newline character."},
                        {"fgets", "Read line from file, keeping newline.", "TLINE = fgets(FID)", "fgets reads next line from file preserving newline."},
                        {"fseek", "Set file position indicator.", "STATUS = fseek(FID, OFFSET, ORIGIN)", "fseek positions file pointer relative to origin."},
                        {"ftell", "Position in file.", "POS = ftell(FID)", "ftell returns the current byte position in file FID."},
                        {"feof", "Test for end-of-file.", "TF = feof(FID)", "feof returns true if file position is at EOF."},
                        {"frewind", "Rewind file pointer to beginning.", "frewind(FID)", "frewind moves file indicator back to start of file."},
                        {"fileread", "Read entire file contents into string.", "STR = fileread(FILENAME)", "fileread loads whole file into text string."},
                        {"tempname", "Unique temporary filename.", "NAME = tempname", "tempname returns unique path for temporary file."},
                        {"tempdir", "Path of temporary directory.", "DIR = tempdir", "tempdir returns system temporary directory path."}
                    }
                }
            }
        },

        // ── 11. general: General purpose commands ────────────────────────
        {
            "general",
            "General purpose commands - workspace and session.",
            {
                {
                    "Workspace and session management.",
                    {
                        {"clear", "Clear variables, functions, and session items from memory.", "clear or clear VAR1 VAR2", "clear removes specified items or all variables from workspace."},
                        {"clc", "Clear command window.", "clc", "clc clears the interactive terminal output."},
                        {"who", "List current variables in workspace.", "who", "who displays variable names in workspace."},
                        {"whos", "List current variables with size, bytes, and class.", "whos", "whos displays detailed table of workspace variables."},
                        {"which", "Locate functions and files.", "which NAME", "which displays the origin (built-in, function, .m file) of NAME."},
                        {"exist", "Check existence of variable, script, function, or class.", "E = exist('name')", "exist returns 1 (var), 2 (file), 5 (builtin), 8 (class)."},
                        {"what", "List Numkit/MATLAB files and toolboxes in folder.", "what or what TOPIC", "what returns list of functions, classes, and packages."},
                        {"inmem", "List functions, classes, and MEX files loaded in memory.", "[M, MEX, C] = inmem", "inmem returns list of loaded functions and classes in session."},
                        {"help", "Display help for functions, categories, or toolboxes.", "help or help TOPIC", "help displays formatted reference documentation."},
                        {"path", "View or change search path.", "P = path", "path displays or modifies search path list."},
                        {"addpath", "Add directories to search path.", "addpath(DIR1, DIR2)", "addpath adds directories to start of search path."},
                        {"rmpath", "Remove directories from search path.", "rmpath(DIR1, DIR2)", "rmpath removes directories from search path."},
                        {"cd", "Change working directory.", "cd NEWDIR", "cd changes current working directory."},
                        {"pwd", "Current working directory.", "DIR = pwd", "pwd returns current working directory path."},
                        {"dir", "List folder contents.", "LIST = dir(PATH)", "dir returns struct array of files in folder."}
                    }
                }
            }
        },

        // ── 12. graphics: Plotting and visualization ─────────────────────
        {
            "graphics",
            "2-D and 3-D plotting and visualization.",
            {
                {
                    "2-D and 3-D line and surface plots.",
                    {
                        {"plot", "2-D line plot.", "plot(X, Y, LINESPEC)", "plot(X, Y) creates 2-D line plots with styles and colors."},
                        {"plot3", "3-D line plot.", "plot3(X, Y, Z)", "plot3(X, Y, Z) creates lines in 3-D Cartesian space."},
                        {"surf", "3-D colored surface plot.", "surf(X, Y, Z)", "surf creates shaded 3-D surface plot."},
                        {"mesh", "3-D wireframe mesh plot.", "mesh(X, Y, Z)", "mesh creates wireframe mesh surface."},
                        {"contour", "Contour plot of matrix.", "contour(Z, LEVELS)", "contour draws isoline contours of surface Z."},
                        {"bar", "Bar graph.", "bar(Y)", "bar creates 2-D vertical bar chart."},
                        {"histogram", "Histogram plot.", "histogram(X, N)", "histogram plots empirical frequency distribution."},
                        {"scatter", "Scatter plot.", "scatter(X, Y, SZ, C)", "scatter draws circles at points (X, Y)."},
                        {"stairs", "Stairstep graph.", "stairs(X, Y)", "stairs plots elements as stairstep sequence."},
                        {"stem", "Discrete sequence stem plot.", "stem(X, Y)", "stem plots data as discrete pins extending from baseline."},
                        {"image", "Display image from matrix.", "image(C)", "image displays matrix C as image with colormap."},
                        {"imagesc", "Scale data and display as image.", "imagesc(C)", "imagesc scales data to colormap range and displays it."},
                        {"imshow", "Display image in figure.", "imshow(I)", "imshow displays image in current figure axes."}
                    }
                },
                {
                    "Figure window and axes control.",
                    {
                        {"figure", "Create figure window.", "H = figure", "figure creates a new figure window or makes existing figure current."},
                        {"gcf", "Get current figure handle.", "H = gcf", "gcf returns handle to current figure."},
                        {"gca", "Get current axes handle.", "AX = gca", "gca returns handle to current axes."},
                        {"clf", "Clear current figure.", "clf", "clf removes all plot objects from current figure."},
                        {"cla", "Clear current axes.", "cla", "cla clears current axes."},
                        {"close", "Close figure window.", "close or close(H)", "close closes specified figure or 'close all' closes all figures."},
                        {"hold", "Retain current plot when adding new plots.", "hold on / hold off", "hold on keeps existing plots when drawing new ones."},
                        {"grid", "Toggle grid lines on axes.", "grid on / grid off", "grid controls display of grid lines on current axes."},
                        {"title", "Add title to current axes.", "title('Text')", "title sets axes title text."},
                        {"xlabel", "Label x-axis.", "xlabel('Text')", "xlabel sets label for horizontal x-axis."},
                        {"ylabel", "Label y-axis.", "ylabel('Text')", "ylabel sets label for vertical y-axis."},
                        {"zlabel", "Label z-axis.", "zlabel('Text')", "zlabel sets label for 3-D z-axis."},
                        {"xlim", "Set or query x-axis limits.", "xlim([XMIN, XMAX])", "xlim sets or returns axis limits in x dimension."},
                        {"ylim", "Set or query y-axis limits.", "ylim([YMIN, YMAX])", "ylim sets or returns axis limits in y dimension."},
                        {"zlim", "Set or query z-axis limits.", "zlim([ZMIN, ZMAX])", "zlim sets or returns axis limits in z dimension."},
                        {"axis", "Set axis limits and aspect ratio.", "axis([XMIN XMAX YMIN YMAX])", "axis controls scaling and appearance of axes."},
                        {"legend", "Add legend to axes.", "legend('label1', 'label2')", "legend displays graph legend for plotted series."},
                        {"colorbar", "Display color bar scale.", "colorbar", "colorbar adds color scale indicator to plot."},
                        {"subplot", "Create axes in tiled positions.", "subplot(M, N, P)", "subplot divides figure into M-by-N grid and selects pane P."},
                        {"view", "Camera line of sight in 3-D.", "view(AZ, EL)", "view sets azimuth and elevation camera angles."}
                    }
                }
            }
        },

        // ── 13. image: Image Processing Toolbox ──────────────────────────
        {
            "image",
            "Image Processing Toolbox.",
            {
                {
                    "Image I/O and display.",
                    {
                        {"imread", "Read image from graphics file.", "A = imread(FILENAME)", "imread reads image from JPG, PNG, BMP, TGA, PNM, TIFF files into uint8/uint16 array."},
                        {"imwrite", "Write image to graphics file.", "imwrite(A, FILENAME)", "imwrite writes image array to disk with automatic codec format detection."},
                        {"imfinfo", "Information about graphics file.", "INFO = imfinfo(FILENAME)", "imfinfo queries image header without full decode."},
                        {"imshow", "Display image in figure window.", "imshow(I)", "imshow displays grayscale or RGB image with aspect ratio preservation."},
                        {"rgb2gray", "Convert RGB image or colormap to grayscale.", "I = rgb2gray(RGB)", "rgb2gray computes weighted luminance 0.2989*R + 0.5870*G + 0.1140*B."},
                        {"gray2rgb", "Convert grayscale image to RGB.", "RGB = gray2rgb(I)", "gray2rgb replicates grayscale intensities into 3 RGB planes."},
                        {"im2double", "Convert image to double precision.", "I2 = im2double(I)", "im2double scales integer images to [0, 1] double range."},
                        {"im2uint8", "Convert image to 8-bit unsigned integers.", "I2 = im2uint8(I)", "im2uint8 quantizes [0, 1] data into [0, 255] uint8 array."}
                    }
                },
                {
                    "Morphological operations.",
                    {
                        {"strel", "Create morphological structuring element.", "SE = strel('disk', R)", "strel creates disk, square, rectangle, diamond structuring elements."},
                        {"imdilate", "Dilate image.", "J = imdilate(I, SE)", "imdilate computes morphological dilation over neighborhood SE."},
                        {"imerode", "Erode image.", "J = imerode(I, SE)", "imerode computes morphological erosion over neighborhood SE."},
                        {"imopen", "Morphologically open image.", "J = imopen(I, SE)", "imopen performs erosion followed by dilation."},
                        {"imclose", "Morphologically close image.", "J = imclose(I, SE)", "imclose performs dilation followed by erosion."},
                        {"imfill", "Fill image regions and holes.", "J = imfill(I, 'holes')", "imfill fills holes in binary and grayscale images."},
                        {"bwareaopen", "Remove small objects from binary image.", "BW2 = bwareaopen(BW, P)", "bwareaopen removes connected components with fewer than P pixels."},
                        {"bwperim", "Find perimeter of objects in binary image.", "BW2 = bwperim(BW)", "bwperim extracts outer 1-pixel boundary of binary regions."},
                        {"bwboundaries", "Trace region boundaries in binary image.", "B = bwboundaries(BW)", "bwboundaries returns boundary pixel coordinates for all regions."}
                    }
                },
                {
                    "Segmentation and region analysis.",
                    {
                        {"edge", "Find edges in intensity image.", "BW = edge(I, 'Canny', THRESH)", "edge detects intensity boundaries using Canny, Sobel, Prewitt, or Roberts filters."},
                        {"bwlabel", "Label connected components in 2-D binary image.", "[L, NUM] = bwlabel(BW, CONN)", "bwlabel labels 4- or 8-connected binary regions with unique integers."},
                        {"bwconncomp", "Find connected components in binary image.", "CC = bwconncomp(BW)", "bwconncomp returns struct of connected component pixel lists."},
                        {"regionprops", "Measure properties of image regions.", "STATS = regionprops(BW, 'Area', 'Centroid')", "regionprops calculates Area, Centroid, BoundingBox, Perimeter, Orientation."},
                        {"imbinarize", "Binarize 2-D grayscale image.", "BW = imbinarize(I, METHOD)", "imbinarize thresholds grayscale image to binary using Otsu or adaptive methods."},
                        {"graythresh", "Global image threshold using Otsu's method.", "LEVEL = graythresh(I)", "graythresh computes optimal threshold minimizing intra-class variance."},
                        {"watershed", "Watershed transform.", "L = watershed(A)", "watershed computes catchment basins and watershed ridge lines."}
                    }
                },
                {
                    "Spatial transformations and filtering.",
                    {
                        {"imresize", "Resize image.", "J = imresize(I, SCALE, METHOD)", "imresize scales image with bilinear, bicubic, or nearest interpolation."},
                        {"imrotate", "Rotate image.", "J = imrotate(I, ANGLE, METHOD)", "imrotate rotates image by specified degrees."},
                        {"imcrop", "Crop image.", "J = imcrop(I, RECT)", "imcrop extracts rectangular subregion [xmin ymin width height]."},
                        {"imfilter", "N-D filtering of multidimensional images.", "J = imfilter(I, H)", "imfilter computes spatial correlation with kernel H."},
                        {"fspecial", "Create predefined 2-D filters.", "H = fspecial('gaussian', SZ, SIGMA)", "fspecial generates gaussian, sobel, prewitt, laplacian, average filters."},
                        {"medfilt2", "2-D median filtering.", "J = medfilt2(I, [M N])", "medfilt2 applies 2-D median noise reduction filter."},
                        {"imhist", "Histogram of image data.", "[COUNTS, BINLOC] = imhist(I)", "imhist computes pixel intensity distribution."},
                        {"histeq", "Enhance contrast using histogram equalization.", "J = histeq(I)", "histeq equalizes image intensity histogram."},
                        {"adapthisteq", "Contrast-limited adaptive histogram equalization (CLAHE).", "J = adapthisteq(I)", "adapthisteq enhances local contrast avoiding over-amplification."}
                    }
                }
            }
        },

        // ── 14. signal: Signal Processing Toolbox ────────────────────────
        {
            "signal",
            "Signal Processing Toolbox.",
            {
                {
                    "Filter design and analysis.",
                    {
                        {"butter", "Butterworth analog and digital filter design.", "[B, A] = butter(N, Wn, TYPE)", "butter designs N-th order lowpass, highpass, bandpass, or bandstop filters."},
                        {"cheby1", "Chebyshev Type I filter design.", "[B, A] = cheby1(N, Rp, Wn)", "cheby1 designs Chebyshev Type I filter with passband ripple."},
                        {"cheby2", "Chebyshev Type II filter design.", "[B, A] = cheby2(N, Rs, Wn)", "cheby2 designs Chebyshev Type II filter with stopband attenuation."},
                        {"ellip", "Elliptic (Cauer) filter design.", "[B, A] = ellip(N, Rp, Rs, Wn)", "ellip designs elliptic filter with equiripple passband and stopband."},
                        {"fir1", "Window-based FIR filter design.", "B = fir1(N, Wn, TYPE)", "fir1 designs N-th order linear-phase FIR filter using window method."},
                        {"freqz", "Frequency response of digital filter.", "[H, W] = freqz(B, A, N)", "freqz computes the complex frequency response H(e^jw)."},
                        {"grpdelay", "Average filter delay (group delay).", "[GD, W] = grpdelay(B, A, N)", "grpdelay computes filter group delay across frequencies."},
                        {"zplane", "Zero-pole plot for discrete-time systems.", "zplane(B, A)", "zplane plots filter transfer function poles and zeros in complex z-plane."}
                    }
                },
                {
                    "Spectral analysis and transforms.",
                    {
                        {"periodogram", "Periodogram power spectral density estimate.", "[Pxx, F] = periodogram(X, WIN, NFFT, Fs)", "periodogram computes nonparametric PSD estimate."},
                        {"pwelch", "Welch's power spectral density estimate.", "[Pxx, F] = pwelch(X, WIN, NOVERLAP, NFFT, Fs)", "pwelch computes averaged modified periodogram PSD."},
                        {"spectrogram", "Spectrogram using short-time Fourier transform (STFT).", "[S, F, T, P] = spectrogram(X, WIN, NOVERLAP, NFFT, Fs)", "spectrogram computes time-frequency spectrogram."},
                        {"findpeaks", "Find local peaks in data.", "[PKS, LOCS] = findpeaks(Y, X)", "findpeaks detects local maxima exceeding threshold and prominence."},
                        {"xcorr", "Cross-correlation and autocorrelation.", "[R, LAGS] = xcorr(X, Y)", "xcorr computes cross-correlation sequence of vectors X and Y."},
                        {"hilbert", "Hilbert transform and analytic signal.", "Z = hilbert(X)", "hilbert computes the discrete-time analytic signal Z = X + i*H(X)."},
                        {"envelope", "Signal envelope extraction.", "[UP, LO] = envelope(X)", "envelope extracts upper and lower signal envelopes."},
                        {"resample", "Resample uniform data at new rate.", "Y = resample(X, P, Q)", "resample resamples signal by rational factor P/Q with polyphase anti-aliasing."}
                    }
                },
                {
                    "Waveforms and signal generators.",
                    {
                        {"chirp", "Swept-frequency cosine signal.", "Y = chirp(T, F0, T1, F1, METHOD)", "chirp generates linear, quadratic, logarithmic frequency sweep."},
                        {"sawtooth", "Sawtooth wave.", "Y = sawtooth(T, WIDTH)", "sawtooth generates periodic sawtooth waveform."},
                        {"square", "Square wave.", "Y = square(T, DUTY)", "square generates periodic square wave with duty cycle."},
                        {"sinc", "Normalized sinc function.", "Y = sinc(X)", "sinc(X) computes sin(pi*X)./(pi*X)."}
                    }
                }
            }
        },

        // ── 15. optim: Optimization Toolbox ──────────────────────────────
        {
            "optim",
            "Optimization Toolbox.",
            {
                {
                    "Unconstrained and constrained nonlinear optimization.",
                    {
                        {"fminunc", "Find minimum of unconstrained multivariable function.", "[X, FVAL] = fminunc(@fun, X0)", "fminunc minimizes objective function using BFGS quasi-Newton method."},
                        {"fmincon", "Find minimum of constrained nonlinear multivariable function.", "[X, FVAL] = fmincon(@fun, X0, A, B, Aeq, Beq, LB, UB, @nonlcon)", "fmincon solves nonlinear constrained optimization problems."},
                        {"fminsearch", "Find minimum of unconstrained multivariable function (derivative-free).", "[X, FVAL] = fminsearch(@fun, X0)", "fminsearch uses Nelder-Mead simplex algorithm."},
                        {"fminbnd", "Find minimum of single-variable function on fixed interval.", "[X, FVAL] = fminbnd(@fun, X1, X2)", "fminbnd finds local minimizer on bounded interval [X1, X2] using golden section search."},
                        {"fzero", "Root of nonlinear function of one variable.", "[X, FVAL] = fzero(@fun, X0)", "fzero solves scalar equation f(x) = 0 using Brent's method."},
                        {"fsolve", "Solve system of nonlinear equations.", "[X, FVAL] = fsolve(@fun, X0)", "fsolve solves F(x) = 0 using Trust-Region Dogleg / Levenberg-Marquardt."},
                        {"lsqnonlin", "Solve nonlinear least-squares problem.", "[X, RESNORM] = lsqnonlin(@fun, X0, LB, UB)", "lsqnonlin minimizes sum of squares of nonlinear vector function."},
                        {"lsqcurvefit", "Fit nonlinear curve to data in least-squares sense.", "[X, RESNORM] = lsqcurvefit(@fun, X0, XDATA, YDATA)", "lsqcurvefit fits model parameters to experimental data."},
                        {"linprog", "Linear programming.", "[X, FVAL] = linprog(F, A, B, Aeq, Beq, LB, UB)", "linprog solves min f'*x subject to linear inequality and equality constraints."},
                        {"quadprog", "Quadratic programming.", "[X, FVAL] = quadprog(H, F, A, B, Aeq, Beq, LB, UB)", "quadprog solves min 0.5*x'*H*x + f'*x subject to linear constraints."}
                    }
                }
            }
        },

        // ── 16. ode: Ordinary Differential Equations ─────────────────────
        {
            "ode",
            "Ordinary Differential Equation (ODE) Solvers.",
            {
                {
                    "Nonstiff and stiff ODE initial value problem solvers.",
                    {
                        {"ode45", "Solve nonstiff differential equations (Runge-Kutta 4th/5th order).", "[T, Y] = ode45(@odefun, TSPAN, Y0)", "ode45 solves y' = f(t, y) with adaptive step size Dormand-Prince (4,5) pair."},
                        {"ode23", "Solve nonstiff differential equations (Bogacki-Shampine 2nd/3rd order).", "[T, Y] = ode23(@odefun, TSPAN, Y0)", "ode23 solves ODEs with adaptive Bogacki-Shampine (2,3) pair."},
                        {"ode113", "Solve nonstiff differential equations (Adams-Bashforth-Moulton).", "[T, Y] = ode113(@odefun, TSPAN, Y0)", "ode113 uses variable-order Adams-Bashforth-Moulton multistep solver."},
                        {"ode15s", "Solve stiff differential equations and DAEs (variable order NDF/BDF).", "[T, Y] = ode15s(@odefun, TSPAN, Y0)", "ode15s solves stiff ODEs and DAEs with numerical differentiation formulas."},
                        {"ode23s", "Solve stiff differential equations (modified Rosenbrock order 2).", "[T, Y] = ode23s(@odefun, TSPAN, Y0)", "ode23s uses Rosenbrock formula for stiff systems with crude tolerances."},
                        {"odeset", "Create or alter ODE options structure.", "OPTIONS = odeset('RelTol', 1e-6, 'AbsTol', 1e-9)", "odeset configures tolerances, step limits, Jacobian, and event functions."},
                        {"odeget", "Extract ODE options parameter.", "VAL = odeget(OPTIONS, 'RelTol')", "odeget retrieves parameter value from options structure."}
                    }
                }
            }
        },

        // ── 17. stats: Statistics and Machine Learning ───────────────────
        {
            "stats",
            "Statistics and Machine Learning Toolbox.",
            {
                {
                    "Regression, fitting and ANOVA.",
                    {
                        {"fitlm", "Fit linear regression model.", "MDL = fitlm(X, Y)", "fitlm estimates linear model coefficients with ANOVA diagnostics and p-values."},
                        {"fitglm", "Fit generalized linear model.", "MDL = fitglm(X, Y, 'Distribution', 'binomial')", "fitglm fits generalized linear models for logistic, Poisson, and gamma regression."},
                        {"nlinfit", "Nonlinear regression model fitting.", "[BETA, R, J] = nlinfit(X, Y, @modelfun, BETA0)", "nlinfit fits nonlinear parameters via Levenberg-Marquardt."},
                        {"regress", "Multiple linear regression using least squares.", "[B, BINT, R, RINT, STATS] = regress(Y, X)", "regress computes OLS coefficients with confidence intervals and R^2."},
                        {"anova1", "One-way analysis of variance.", "[P, TABLE, STATS] = anova1(Y, GROUP)", "anova1 performs one-way ANOVA comparing group means."}
                    }
                },
                {
                    "Hypothesis tests.",
                    {
                        {"ttest", "One-sample or paired-sample t-test.", "[H, P, CI, STATS] = ttest(X, M)", "ttest tests null hypothesis that sample mean equals M."},
                        {"ttest2", "Two-sample t-test with equal or unequal variances.", "[H, P, CI, STATS] = ttest2(X, Y)", "ttest2 performs independent two-sample t-test."},
                        {"ztest", "Z-test for known population standard deviation.", "[H, P, CI, STATS] = ztest(X, M, SIGMA)", "ztest tests hypothesis with known variance SIGMA^2."},
                        {"kstest", "One-sample Kolmogorov-Smirnov test.", "[H, P, KSSTAT] = kstest(X)", "kstest evaluates empirical distribution vs reference distribution."},
                        {"ranksum", "Wilcoxon rank-sum test for two independent samples.", "[P, H, STATS] = ranksum(X, Y)", "ranksum performs Mann-Whitney U nonparametric rank-sum test."}
                    }
                },
                {
                    "Probability distributions and random generators.",
                    {
                        {"normpdf", "Normal probability density function.", "Y = normpdf(X, MU, SIGMA)", "normpdf computes Gaussian probability density."},
                        {"normcdf", "Normal cumulative distribution function.", "P = normcdf(X, MU, SIGMA)", "normcdf computes standard normal CDF."},
                        {"norminv", "Normal inverse cumulative distribution function.", "X = norminv(P, MU, SIGMA)", "norminv computes quantiles of normal distribution."},
                        {"normrnd", "Random numbers from normal distribution.", "R = normrnd(MU, SIGMA, M, N)", "normrnd generates random matrices from Gaussian distribution."},
                        {"tpdf", "Student's t probability density function.", "Y = tpdf(X, NU)", "tpdf computes Student's t distribution PDF with NU degrees of freedom."},
                        {"tcdf", "Student's t cumulative distribution function.", "P = tcdf(X, NU)", "tcdf computes Student's t distribution CDF."},
                        {"tinv", "Student's t inverse cumulative distribution function.", "X = tinv(P, NU)", "tinv computes quantiles of Student's t distribution."},
                        {"chi2pdf", "Chi-square probability density function.", "Y = chi2pdf(X, NU)", "chi2pdf computes chi-square distribution density with NU degrees of freedom."},
                        {"chi2cdf", "Chi-square cumulative distribution function.", "P = chi2cdf(X, NU)", "chi2cdf computes chi-square distribution CDF."},
                        {"fpdf", "F probability density function.", "Y = fpdf(X, NU1, NU2)", "fpdf computes Snedecor's F distribution density."},
                        {"fcdf", "F cumulative distribution function.", "P = fcdf(X, NU1, NU2)", "fcdf computes Snedecor's F distribution CDF."},
                        {"poissrnd", "Random numbers from Poisson distribution.", "R = poissrnd(LAMBDA, M, N)", "poissrnd generates random numbers from Poisson distribution."},
                        {"binornd", "Random numbers from binomial distribution.", "R = binornd(N, P, M, K)", "binornd generates random numbers from binomial distribution."}
                    }
                }
            }
        },

        // ── 18. control: Control System Toolbox ───────────────────────────
        {
            "control",
            "Control System Toolbox.",
            {
                {
                    "Linear time-invariant (LTI) models.",
                    {
                        {"tf", "Create transfer function model.", "SYS = tf(NUM, DEN)", "tf creates continuous- or discrete-time transfer function system SYS(s) = NUM(s)/DEN(s)."},
                        {"ss", "Create state-space model.", "SYS = ss(A, B, C, D)", "ss creates state-space dynamic system model dx/dt = A*x + B*u, y = C*x + D*u."},
                        {"zpk", "Create zero-pole-gain model.", "SYS = zpk(Z, P, K)", "zpk creates LTI model from roots Z, poles P, and scalar gain K."},
                        {"pid", "Create PID controller model.", "C = pid(Kp, Ki, Kd, Tf)", "pid creates continuous-time parallel PID controller C(s) = Kp + Ki/s + Kd*s/(Tf*s+1)."},
                        {"feedback", "Feedback connection of two models.", "SYS = feedback(SYS1, SYS2, SIGN)", "feedback computes closed-loop interconnection SYS1 / (1 + SYS1*SYS2)."},
                        {"series", "Series connection of two models.", "SYS = series(SYS1, SYS2)", "series computes cascade connection SYS1 * SYS2."},
                        {"parallel", "Parallel connection of two models.", "SYS = parallel(SYS1, SYS2)", "parallel computes parallel sum SYS1 + SYS2."}
                    }
                },
                {
                    "Time and frequency response.",
                    {
                        {"step", "Step response of dynamic system.", "[Y, T] = step(SYS)", "step computes and plots transient response to unit step input."},
                        {"impulse", "Impulse response of dynamic system.", "[Y, T] = impulse(SYS)", "impulse computes and plots transient response to unit Dirac impulse."},
                        {"bode", "Bode frequency response plot.", "[MAG, PHASE, W] = bode(SYS)", "bode computes magnitude (dB) and phase (deg) frequency response curves."},
                        {"nyquist", "Nyquist plot of dynamic system.", "[RE, IM, W] = nyquist(SYS)", "nyquist plots complex frequency response in polar coordinates."},
                        {"margin", "Gain and phase margins and crossover frequencies.", "[Gm, Pm, Wcg, Wcp] = margin(SYS)", "margin calculates stability margins of open-loop system."}
                    }
                }
            }
        }
    };

    categoryIndex_.clear();
    functionIndex_.clear();

    for (size_t i = 0; i < categories_.size(); ++i) {
        const auto &cat = categories_[i];
        categoryIndex_[toLowerStr(cat.name)] = i;

        for (const auto &sec : cat.sections) {
            for (const auto &entry : sec.entries) {
                functionIndex_[toLowerStr(entry.name)] = entry;
            }
        }
    }
}

} // namespace numkit::runtime
