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
    if (name == "io" || name == "file" || name == "files") name = "iofun";
    if (name == "plots" || name == "plot") name = "graphics";
    if (name == "operators" || name == "operator" || name == "arithmetic") name = "ops";
    if (name == "general" || name == "language" || name == "errors" || name == "diag" || name == "diagnostics") name = "lang";
    if (name == "communications" || name == "comms") name = "comm";
    if (name == "wavelets") name = "wavelet";
    if (name == "sound" || name == "sounds") name = "audio";

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
    os << "  Fundamentals:\n";
    for (size_t i = 0; i < 12 && i < categories_.size(); ++i) {
        os << "    " << padRight(categories_[i].name, 14) << "- " << categories_[i].title << "\n";
    }
    os << "\n  Toolboxes:\n";
    for (size_t i = 12; i < categories_.size(); ++i) {
        os << "    " << padRight(categories_[i].name, 14) << "- " << categories_[i].title << "\n";
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

    os << " " << upperName << " " << entry->summary << "\n\n";
    os << "    " << entry->signature << "\n\n";
    os << "    " << entry->doc << "\n";
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
                        {"randi", "Pseudorandom integers from uniform discrete distribution.", "randi(IMAX, M, N)", "randi(IMAX, M, N) returns random integers between 1 and IMAX."},
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
                        {"find", "Find indices and values of non-zero elements.", "[row, col, v] = find(X)", "find returns linear or subscript indices of non-zero elements."},
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
                        {"atan2", "Four-quadrant inverse tangent.", "Y = atan2(Y, X)", "atan2(Y, X) computes four-quadrant arctangent in [-pi, pi]."},
                        {"atan2d", "Four-quadrant inverse tangent in degrees.", "Y = atan2d(Y, X)", "atan2d computes four-quadrant arctangent in degrees."},
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
                        {"exp", "Exponential e^x.", "Y = exp(X)", "exp(X) computes the natural exponential e^x for each element."},
                        {"log", "Natural logarithm ln(x).", "Y = log(X)", "log(X) computes the natural logarithm ln(x)."},
                        {"log10", "Common (base 10) logarithm.", "Y = log10(X)", "log10(X) computes the base-10 logarithm."},
                        {"log2", "Base 2 logarithm and dissect floating point.", "Y = log2(X)", "log2(X) computes the base-2 logarithm."},
                        {"sqrt", "Square root.", "Y = sqrt(X)", "sqrt(X) computes the square root of each element."},
                        {"nthroot", "Real nth root of real numbers.", "Y = nthroot(X, N)", "nthroot returns the real nth root of elements of X."},
                        {"nextpow2", "Next higher power of 2.", "P = nextpow2(N)", "nextpow2 returns the smallest integer P such that 2^P >= abs(N)."},
                        {"pow2", "Base 2 power and scale floating point.", "Y = pow2(X)", "pow2(X) computes 2.^X."}
                    }
                },
                {
                    "Complex operations.",
                    {
                        {"abs", "Absolute value and complex magnitude.", "Y = abs(X)", "abs(X) computes the absolute value or complex magnitude |z|."},
                        {"angle", "Phase angle of complex numbers.", "P = angle(Z)", "angle(Z) returns the phase angles in radians in [-pi, pi]."},
                        {"complex", "Construct complex from real and imaginary.", "Z = complex(A, B)", "complex(A, B) creates complex array A + B*i."},
                        {"conj", "Complex conjugate.", "Y = conj(X)", "conj(X) reverses the sign of the imaginary parts."},
                        {"real", "Real part of complex number.", "Y = real(X)", "real(X) returns the real part of complex array X."},
                        {"imag", "Imaginary part of complex number.", "Y = imag(X)", "imag(X) returns the imaginary part of complex array X."},
                        {"isreal", "True for real array.", "TF = isreal(X)", "isreal(X) returns true if X does not contain complex elements."},
                        {"sign", "Signum function.", "Y = sign(X)", "sign(X) returns 1 for x>0, -1 for x<0, 0 for x=0, and x/abs(x) for complex x."},
                        {"unwrap", "Unwrap phase angles.", "Q = unwrap(P)", "unwrap corrects phase angles by adding multiples of +/- 2*pi."}
                    }
                },
                {
                    "Rounding and remainders.",
                    {
                        {"floor", "Round toward minus infinity.", "Y = floor(X)", "floor(X) rounds elements to the nearest integers <= X."},
                        {"ceil", "Round toward plus infinity.", "Y = ceil(X)", "ceil(X) rounds elements to the nearest integers >= X."},
                        {"round", "Round to nearest integer.", "Y = round(X)", "round(X) rounds elements to nearest integers (half away from zero)."},
                        {"fix", "Round toward zero.", "Y = fix(X)", "fix(X) rounds elements to nearest integers toward zero."},
                        {"mod", "Modulus after division.", "M = mod(X, Y)", "mod(X, Y) returns X - Y.*floor(X./Y)."},
                        {"rem", "Remainder after division.", "R = rem(X, Y)", "rem(X, Y) returns X - Y.*fix(X./Y)."}
                    }
                }
            }
        },

        // ── 3. ops: Operators and elementary operations ──────────────────
        {
            "ops",
            "Operators and elementary operations.",
            {
                {
                    "Arithmetic operators.",
                    {
                        {"plus", "Addition (+).", "C = plus(A, B) or A + B", "plus adds corresponding elements of arrays with broadcasting."},
                        {"uplus", "Unary plus (+).", "B = uplus(A) or +A", "uplus returns unchanged copy of numeric array."},
                        {"minus", "Subtraction (-).", "C = minus(A, B) or A - B", "minus subtracts B from A element-by-element."},
                        {"uminus", "Unary minus (-).", "B = uminus(A) or -A", "uminus negates each element of array A."},
                        {"times", "Element-wise multiplication (.*).", "C = times(A, B) or A .* B", "times multiplies arrays element-by-element with broadcasting."},
                        {"rdivide", "Right array division (./).", "C = rdivide(A, B) or A ./ B", "rdivide divides A by B element-by-element."},
                        {"ldivide", "Left array division (.\\\\).", "C = ldivide(A, B) or A .\\\\ B", "ldivide divides B by A element-by-element."},
                        {"power", "Element-wise power (.^).", "C = power(A, B) or A .^ B", "power raises elements of A to powers in B."},
                        {"mtimes", "Matrix multiplication (*).", "C = mtimes(A, B) or A * B", "mtimes computes algebraic matrix product of A and B."},
                        {"mrdivide", "Matrix right division (/): B / A = B * inv(A).", "X = mrdivide(B, A) or B / A", "mrdivide solves X * A = B for X."},
                        {"mldivide", "Matrix left division (\\\\): A \\\\ B = inv(A) * B.", "X = mldivide(A, B) or A \\\\ B", "mldivide solves linear system A * X = B."},
                        {"mpower", "Matrix power (^).", "C = mpower(A, B) or A ^ B", "mpower computes matrix exponential or repeated multiplication."},
                        {"pagemtimes", "Page-wise matrix multiplication.", "C = pagemtimes(A, B)", "pagemtimes evaluates matrix products page-by-page across N-D arrays."},
                        {"transpose", "Array non-conjugate transpose (.').", "B = transpose(A) or A.'", "transpose interchanges row and column indices of array."},
                        {"ctranspose", "Complex conjugate transpose (').", "B = ctranspose(A) or A'", "ctranspose computes Hermitian transpose (conjugate transpose)."}
                    }
                },
                {
                    "Relational operators.",
                    {
                        {"eq", "Equal (==).", "TF = eq(A, B) or A == B", "eq compares elements for equality, returning logical array."},
                        {"ne", "Not equal (~=).", "TF = ne(A, B) or A ~= B", "ne tests if corresponding elements are not equal."},
                        {"lt", "Less than (<).", "TF = lt(A, B) or A < B", "lt tests if A is strictly less than B element-wise."},
                        {"gt", "Greater than (>).", "TF = gt(A, B) or A > B", "gt tests if A is strictly greater than B element-wise."},
                        {"le", "Less than or equal (<=).", "TF = le(A, B) or A <= B", "le tests if A is less than or equal to B."},
                        {"ge", "Greater than or equal (>=).", "TF = ge(A, B) or A >= B", "ge tests if A is greater than or equal to B."}
                    }
                },
                {
                    "Logical and bitwise operators.",
                    {
                        {"and", "Logical AND (&).", "C = and(A, B) or A & B", "and performs element-wise logical AND."},
                        {"or", "Logical OR (|).", "C = or(A, B) or A | B", "or performs element-wise logical OR."},
                        {"not", "Logical NOT (~).", "B = not(A) or ~A", "not negates logical values of elements."},
                        {"xor", "Logical EXCLUSIVE-OR.", "C = xor(A, B)", "xor performs element-wise exclusive OR."},
                        {"any", "True if any element of vector is non-zero.", "TF = any(X, DIM)", "any tests if any elements along DIM are non-zero."},
                        {"all", "True if all elements of vector are non-zero.", "TF = all(X, DIM)", "all tests if all elements along DIM are non-zero."},
                        {"bitand", "Bitwise AND.", "C = bitand(A, B)", "bitand computes bitwise AND of integer inputs."},
                        {"bitor", "Bitwise OR.", "C = bitor(A, B)", "bitor computes bitwise OR of integer inputs."},
                        {"bitxor", "Bitwise XOR.", "C = bitxor(A, B)", "bitxor computes bitwise exclusive OR of integers."},
                        {"bitcmp", "Bitwise complement.", "C = bitcmp(A, N)", "bitcmp returns bitwise complement of unsigned integers."},
                        {"bitshift", "Bitwise shift.", "C = bitshift(A, K)", "bitshift shifts integer bits left (K>0) or right (K<0)."},
                        {"bitget", "Get bit at specified position.", "B = bitget(A, POS)", "bitget extracts bit values at bit position POS."},
                        {"bitset", "Set bit at specified position.", "C = bitset(A, POS, V)", "bitset sets bit at position POS to V (0 or 1)."}
                    }
                }
            }
        },

        // ── 4. matfun: Matrix functions - numerical linear algebra ───────
        {
            "matfun",
            "Matrix functions - numerical linear algebra.",
            {
                {
                    "Matrix analysis.",
                    {
                        {"norm", "Matrix or vector norm.", "N = norm(A, P)", "norm(A) calculates 1-, 2-, Inf-, or Frobenius norm."},
                        {"normest", "Estimate 2-norm of matrix.", "E = normest(A)", "normest estimates matrix 2-norm using power iteration."},
                        {"rank", "Matrix rank.", "K = rank(A, TOL)", "rank returns number of singular values greater than tolerance."},
                        {"det", "Matrix determinant.", "D = det(A)", "det computes algebraic determinant of square matrix."},
                        {"trace", "Sum of diagonal elements.", "T = trace(A)", "trace computes sum of main diagonal elements."},
                        {"null", "Null space of matrix.", "Z = null(A)", "null finds orthonormal basis for null space A*Z = 0."},
                        {"orth", "Orthonormal basis for range of matrix.", "Q = orth(A)", "orth computes orthonormal basis for range space of A."},
                        {"rref", "Reduced row echelon form.", "[R, JB] = rref(A)", "rref produces reduced row echelon form using Gauss-Jordan elimination."},
                        {"subspace", "Angle between two subspaces.", "THETA = subspace(A, B)", "subspace calculates angle in radians between column spaces of A and B."}
                    }
                },
                {
                    "Linear equations.",
                    {
                        {"inv", "Matrix inverse.", "Y = inv(X)", "inv(X) computes exact matrix inverse X^(-1)."},
                        {"pinv", "Moore-Penrose pseudoinverse.", "Y = pinv(X, TOL)", "pinv calculates Moore-Penrose pseudoinverse via SVD."},
                        {"linsolve", "Solve linear system of equations.", "X = linsolve(A, B, OPTS)", "linsolve solves linear system with optional structure flags."},
                        {"cond", "Condition number with respect to inversion.", "C = cond(A, P)", "cond computes matrix 2-norm condition number ratio."},
                        {"rcond", "Reciprocal condition number estimate.", "R = rcond(A)", "rcond returns 1-norm reciprocal condition estimate in (0, 1]."}
                    }
                },
                {
                    "Matrix factorizations.",
                    {
                        {"lu", "LU matrix factorization.", "[L, U, P] = lu(A)", "lu factors square or rectangular matrix into permutation, unit lower and upper triangular."},
                        {"qr", "QR matrix factorization.", "[Q, R, P] = qr(A)", "qr factors matrix into orthogonal Q and upper triangular R."},
                        {"chol", "Cholesky factorization of positive definite matrix.", "R = chol(A)", "chol factors Hermitian positive-definite matrix into R'*R."},
                        {"cholupdate", "Rank 1 update to Cholesky factorization.", "R1 = cholupdate(R, X, OP)", "cholupdate updates Cholesky factor R after rank-1 modification."},
                        {"svd", "Singular value decomposition.", "[U, S, V] = svd(A)", "svd computes singular values and unitary matrices A = U*S*V'."},
                        {"gsvd", "Generalized singular value decomposition.", "[U, V, X, C, S] = gsvd(A, B)", "gsvd computes simultaneous SVD of matrices A and B."},
                        {"eig", "Eigenvalues and eigenvectors.", "[V, D] = eig(A)", "eig calculates eigenvalues and right eigenvectors of square matrix."},
                        {"schur", "Schur decomposition.", "[U, T] = schur(A)", "schur factors matrix into unitary U and quasi-upper-triangular T."},
                        {"ordschur", "Reorder eigenvalues in Schur factorization.", "[US, TS] = ordschur(U, T, SELECT)", "ordschur reorders diagonal blocks of Schur form."},
                        {"hess", "Hessenberg form of matrix.", "[P, H] = hess(A)", "hess computes unitary similarity reduction to upper Hessenberg form."},
                        {"qz", "QZ factorization for generalized eigenvalues.", "[AA, BB, Q, Z, V, W] = qz(A, B)", "qz computes generalized Schur decomposition."},
                        {"ordqz", "Reorder eigenvalues in QZ factorization.", "[AAS, BBS, QS, ZS] = ordqz(AA, BB, Q, Z, SELECT)", "ordqz reorders generalized eigenvalues in QZ form."},
                        {"rsf2csf", "Real to complex Schur form.", "[U, T] = rsf2csf(UR, TR)", "rsf2csf converts real Schur form to complex triangular Schur form."}
                    }
                },
                {
                    "Matrix functions.",
                    {
                        {"expm", "Matrix exponential e^A.", "E = expm(A)", "expm computes matrix exponential via Pade approximation with scaling."},
                        {"logm", "Matrix logarithm ln(A).", "L = logm(A)", "logm computes principal matrix logarithm."},
                        {"sqrtm", "Matrix square root A^(1/2).", "S = sqrtm(A)", "sqrtm computes principal matrix square root."},
                        {"funm", "Evaluate general matrix function.", "F = funm(A, @fun)", "funm evaluates analytic scalar function on square matrix via Parlett recurrence."},
                        {"kron", "Kronecker tensor product.", "K = kron(A, B)", "kron computes Kronecker matrix tensor product."},
                        {"sylvester", "Solve Sylvester matrix equation A*X + X*B = C.", "X = sylvester(A, B, C)", "sylvester solves Sylvester matrix equation using Bartels-Stewart algorithm."}
                    }
                }
            }
        },

        // ── 5. datafun: Data analysis and Fourier transforms ─────────────
        {
            "datafun",
            "Data analysis, summary statistics and Fourier transforms.",
            {
                {
                    "Basic summary statistics.",
                    {
                        {"max", "Largest elements in array.", "M = max(A, [], DIM)", "max returns maximum element values along dimension DIM."},
                        {"min", "Smallest elements in array.", "M = min(A, [], DIM)", "min returns minimum element values along dimension DIM."},
                        {"mean", "Average or mean value of array.", "M = mean(A, DIM)", "mean computes arithmetic mean along specified dimension."},
                        {"median", "Median value of array.", "M = median(A, DIM)", "median computes sample median value along dimension."},
                        {"std", "Standard deviation.", "S = std(A, W, DIM)", "std computes sample standard deviation (W=0 for N-1 normalization)."},
                        {"var", "Variance.", "V = var(A, W, DIM)", "var computes sample variance along specified dimension."},
                        {"sum", "Sum of array elements.", "S = sum(A, DIM)", "sum computes sum of elements along dimension DIM."},
                        {"prod", "Product of array elements.", "P = prod(A, DIM)", "prod computes product of elements along dimension DIM."},
                        {"cumsum", "Cumulative sum of elements.", "S = cumsum(A, DIM)", "cumsum computes running cumulative sum along dimension."},
                        {"cumprod", "Cumulative product of elements.", "P = cumprod(A, DIM)", "cumprod computes running cumulative product along dimension."},
                        {"cummax", "Cumulative maximum.", "M = cummax(A, DIM)", "cummax computes running maximum along dimension."},
                        {"cummin", "Cumulative minimum.", "M = cummin(A, DIM)", "cummin computes running minimum along dimension."},
                        {"diff", "Differences and approximate derivatives.", "Y = diff(X, N, DIM)", "diff computes Nth forward difference adjacent elements."},
                        {"gradient", "Numerical gradient.", "[FX, FY] = gradient(F)", "gradient computes central differences numerical gradient."},
                        {"trapz", "Trapezoidal numerical integration.", "Z = trapz(X, Y)", "trapz computes trapezoidal integration over discrete points."},
                        {"cumtrapz", "Cumulative trapezoidal integration.", "Z = cumtrapz(X, Y)", "cumtrapz computes cumulative trapezoidal integral."},
                        {"corrcoef", "Correlation coefficients matrix.", "R = corrcoef(X, Y)", "corrcoef computes Pearson linear correlation coefficient matrix."},
                        {"cov", "Covariance matrix.", "C = cov(X, Y)", "cov computes sample covariance matrix."},
                        {"sort", "Sort array elements in ascending or descending order.", "B = sort(A, DIM, MODE)", "sort sorts elements along dimension DIM in 'ascend' or 'descend' order."},
                        {"sortrows", "Sort matrix rows based on keys.", "B = sortrows(A, COL)", "sortrows sorts rows of matrix according to specified columns."},
                        {"issorted", "Determine if array is sorted.", "TF = issorted(A, DIM)", "issorted returns true if elements are in sorted order."},
                        {"accumarray", "Construct array by accumulation.", "A = accumarray(SUBS, VALS, SZ, FUN)", "accumarray groups values by subscript indices and reduces via FUN."}
                    }
                },
                {
                    "Fourier transforms.",
                    {
                        {"fft", "1-D Fast Fourier Transform.", "Y = fft(X, N, DIM)", "fft computes discrete Fourier transform using Cooley-Tukey FFT algorithm."},
                        {"ifft", "1-D Inverse Fast Fourier Transform.", "Y = ifft(X, N, DIM)", "ifft computes inverse discrete Fourier transform."},
                        {"fft2", "2-D Fast Fourier Transform.", "Y = fft2(X, M, N)", "fft2 computes 2-D discrete Fourier transform."},
                        {"ifft2", "2-D Inverse Fast Fourier Transform.", "Y = ifft2(X, M, N)", "ifft2 computes 2-D inverse discrete Fourier transform."},
                        {"fftn", "N-D Fast Fourier Transform.", "Y = fftn(X, SZ)", "fftn computes N-D discrete Fourier transform."},
                        {"ifftn", "N-D Inverse Fast Fourier Transform.", "Y = ifftn(X, SZ)", "ifftn computes N-D inverse discrete Fourier transform."},
                        {"fftshift", "Shift zero-frequency component to center of spectrum.", "Y = fftshift(X, DIM)", "fftshift swaps left and right halves of transform."},
                        {"ifftshift", "Inverse zero-frequency component shift.", "Y = ifftshift(X, DIM)", "ifftshift undoes the effect of fftshift."}
                    }
                }
            }
        },

        // ── 6. specfun: Special mathematical functions ───────────────────
        {
            "specfun",
            "Special mathematical functions.",
            {
                {
                    "Bessel and Airy functions.",
                    {
                        {"besselj", "Bessel function of the first kind J_nu(z).", "J = besselj(NU, Z)", "besselj computes Bessel functions of the first kind."},
                        {"bessely", "Bessel function of the second kind Y_nu(z).", "Y = bessely(NU, Z)", "bessely computes Bessel functions of the second kind (Neumann)."},
                        {"besseli", "Modified Bessel function of first kind I_nu(z).", "I = besseli(NU, Z)", "besseli computes modified Bessel functions of first kind."},
                        {"besselk", "Modified Bessel function of second kind K_nu(z).", "K = besselk(NU, Z)", "besselk computes modified Bessel functions of second kind."},
                        {"besselh", "Bessel functions of the third kind (Hankel).", "H = besselh(M, NU, Z)", "besselh computes Hankel functions H1 and H2."},
                        {"airy", "Airy functions Ai(x) and Bi(x).", "W = airy(K, Z)", "airy computes Airy functions and their derivatives."}
                    }
                },
                {
                    "Gamma, Beta and Error functions.",
                    {
                        {"gamma", "Gamma function Gamma(z).", "Y = gamma(X)", "gamma(X) evaluates the complete Gamma function."},
                        {"gammainc", "Incomplete gamma function.", "Y = gammainc(X, A, TAIL)", "gammainc evaluates regularized lower or upper incomplete gamma function."},
                        {"gammaln", "Logarithm of gamma function ln(Gamma(x)).", "Y = gammaln(X)", "gammaln computes logarithm of Gamma function avoiding overflow."},
                        {"psi", "Digamma and polygamma functions.", "Y = psi(K, X)", "psi computes derivatives of the logarithm of the gamma function."},
                        {"beta", "Beta function B(z, w).", "Y = beta(Z, W)", "beta evaluates Beta function B(z, w) = Gamma(z)*Gamma(w)/Gamma(z+w)."},
                        {"betainc", "Incomplete beta function.", "Y = betainc(X, A, B, TAIL)", "betainc evaluates regularized incomplete beta function."},
                        {"betaln", "Logarithm of beta function ln(B(z,w)).", "Y = betaln(Z, W)", "betaln computes logarithm of Beta function."},
                        {"erf", "Error function.", "Y = erf(X)", "erf(X) computes the Gauss error function 2/sqrt(pi) * int(0..x, exp(-t^2) dt)."},
                        {"erfc", "Complementary error function 1 - erf(x).", "Y = erfc(X)", "erfc(X) evaluates 1 - erf(X) with high precision for large X."},
                        {"erfinv", "Inverse error function.", "Y = erfinv(X)", "erfinv computes the inverse error function such that erf(erfinv(x)) = x."},
                        {"erfcinv", "Inverse complementary error function.", "Y = erfcinv(X)", "erfcinv computes inverse complementary error function."},
                        {"ellipke", "Complete elliptic integrals of first and second kind.", "[K, E] = ellipke(M)", "ellipke calculates complete elliptic integrals K(m) and E(m)."},
                        {"ellipj", "Jacobi elliptic functions sn, cn, dn.", "[SN, CN, DN] = ellipj(U, M)", "ellipj computes Jacobian elliptic functions."}
                    }
                },
                {
                    "Number theory.",
                    {
                        {"gcd", "Greatest common divisor.", "[G, C, D] = gcd(A, B)", "gcd computes greatest common divisor of integer values."},
                        {"lcm", "Least common multiple.", "L = lcm(A, B)", "lcm computes least common multiple of integers."},
                        {"factor", "Prime factor decomposition.", "F = factor(N)", "factor returns prime factors of positive integer N."},
                        {"isprime", "True for prime numbers.", "TF = isprime(X)", "isprime returns true for prime integers."},
                        {"primes", "Generate list of prime numbers <= N.", "P = primes(N)", "primes returns row vector of all primes less than or equal to N."},
                        {"nchoosek", "Binomial coefficient (n choose k) and combinations.", "C = nchoosek(N, K)", "nchoosek computes n! / (k! * (n-k)!)."},
                        {"factorial", "Factorial function n!.", "F = factorial(N)", "factorial computes product of integers 1 to N."}
                    }
                }
            }
        },

        // ── 7. polyfun: Polynomials and interpolation ────────────────────
        {
            "polyfun",
            "Polynomials, interpolation and numerical integration.",
            {
                {
                    "Polynomials.",
                    {
                        {"poly", "Polynomial with specified roots or characteristic polynomial.", "P = poly(R)", "poly returns polynomial coefficients given roots or matrix."},
                        {"roots", "Polynomial roots.", "R = roots(P)", "roots calculates complex roots of polynomial P via companion matrix eigenvalues."},
                        {"polyval", "Evaluate polynomial.", "Y = polyval(P, X)", "polyval evaluates polynomial P at points X using Horner's method."},
                        {"polyvalm", "Evaluate polynomial with matrix argument.", "Y = polyvalm(P, A)", "polyvalm evaluates matrix polynomial P(A)."},
                        {"polyfit", "Polynomial curve fitting (least squares).", "[P, S, MU] = polyfit(X, Y, N)", "polyfit finds least-squares polynomial fit of degree N."},
                        {"polyder", "Differentiate polynomial or polynomial product.", "K = polyder(P)", "polyder calculates derivative of polynomial P."},
                        {"polyint", "Integrate polynomial analytically.", "K = polyint(P, C)", "polyint returns analytical integral of polynomial with constant C."},
                        {"conv", "Convolution and polynomial multiplication.", "C = conv(A, B, SHAPE)", "conv multiplies polynomials or computes 1-D convolution."},
                        {"deconv", "Deconvolution and polynomial division.", "[Q, R] = deconv(B, A)", "deconv computes quotient and remainder polynomials B = A*Q + R."},
                        {"residue", "Partial fraction expansion (residues and poles).", "[R, P, K] = residue(B, A)", "residue converts rational transfer function to residue partial fractions."}
                    }
                },
                {
                    "Interpolation.",
                    {
                        {"interp1", "1-D data interpolation.", "YI = interp1(X, Y, XI, METHOD)", "interp1 interpolates 1-D data via 'linear', 'spline', 'pchip', 'nearest'."},
                        {"interp2", "2-D grid data interpolation.", "ZI = interp2(X, Y, Z, XI, YI, METHOD)", "interp2 interpolates 2-D gridded surface data."},
                        {"interp3", "3-D volume data interpolation.", "VI = interp3(X, Y, Z, V, XI, YI, ZI)", "interp3 performs tri-linear or cubic 3-D volume interpolation."},
                        {"interpn", "N-D gridded interpolation.", "VI = interpn(X1, X2, ..., V, Y1, Y2, ...)", "interpn performs multidimensional interpolation on regular grids."},
                        {"griddata", "Interpolate scattered data in 2-D.", "ZI = griddata(X, Y, Z, XI, YI, METHOD)", "griddata fits surface to scattered data points via Delaunay triangulation."},
                        {"spline", "Cubic spline data interpolation.", "YY = spline(X, Y, XX)", "spline evaluates cubic spline with not-a-knot end conditions."},
                        {"pchip", "Piecewise Cubic Hermite Interpolating Polynomial.", "YY = pchip(X, Y, XX)", "pchip constructs shape-preserving monotonic cubic interpolant."}
                    }
                }
            }
        },

        // ── 8. strfun: Character and string manipulation ─────────────────
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
                        {"orderfields", "Order fields of structure array.", "S2 = orderfields(S1)", "orderfields sorts fields alphabetically or according to template."},
                        {"cellfun", "Apply function to each cell in array.", "[A, B] = cellfun(@fun, C)", "cellfun evaluates function over all elements of cell array."},
                        {"arrayfun", "Apply function to each element of array.", "B = arrayfun(@fun, A)", "arrayfun evaluates function over array elements."},
                        {"structfun", "Apply function to each field in structure.", "B = structfun(@fun, S)", "structfun applies function to each field of structure."}
                    }
                },
                {
                    "Object model and class introspection.",
                    {
                        {"class", "Class name of object.", "CN = class(OBJ)", "class(OBJ) returns class name string."},
                        {"isa", "Determine if input is of specified class.", "TF = isa(OBJ, 'classname')", "isa tests if object belongs to class or superclass."},
                        {"isnumeric", "True for numeric array.", "TF = isnumeric(A)", "isnumeric returns true if input contains numeric elements."},
                        {"ischar", "True for character array.", "TF = ischar(A)", "ischar returns true for 1-D/2-D character matrices."},
                        {"isstring", "True for string array.", "TF = isstring(A)", "isstring returns true for string arrays."},
                        {"islogical", "True for logical array.", "TF = islogical(A)", "islogical returns true for boolean arrays."},
                        {"cast", "Cast variable to different data type.", "B = cast(A, 'newclass')", "cast converts variable to specified numeric or logical class."},
                        {"isobject", "True for MATLAB OOP objects.", "TF = isobject(OBJ)", "isobject returns true for instances of classdef classes."},
                        {"isprop", "True if object declares property.", "TF = isprop(OBJ, 'propname')", "isprop tests if class has specified property."},
                        {"ismethod", "True if object declares method.", "TF = ismethod(OBJ, 'methodname')", "ismethod tests if class declares specified method."},
                        {"methods", "List class methods.", "M = methods('classname')", "methods returns cellstr of all accessible methods."},
                        {"properties", "List class property names.", "P = properties('classname')", "properties returns cellstr of public property names."},
                        {"containers.Map", "Key-value associative map container.", "M = containers.Map(keys, values)", "containers.Map creates key-value map collection."}
                    }
                }
            }
        },

        // ── 10. timefun: Time and dates ──────────────────────────────────
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

        // ── 11. lang: Language syntax, diagnostics and session ───────────
        {
            "lang",
            "Language syntax, diagnostics and session.",
            {
                {
                    "Language syntax and keywords.",
                    {
                        {"iskeyword", "True for MATLAB keywords.", "TF = iskeyword('name')", "iskeyword returns true if name is a reserved keyword."},
                        {"isvarname", "True for valid variable names.", "TF = isvarname('name')", "isvarname tests if string conforms to identifier rules."},
                        {"getenv", "Get environment variable value.", "VAL = getenv('NAME')", "getenv reads operating system environment variable."},
                        {"setenv", "Set environment variable.", "setenv('NAME', 'VAL')", "setenv assigns value to environment variable in current process."}
                    }
                },
                {
                    "Error handling and diagnostics.",
                    {
                        {"error", "Throw exception and terminate function.", "error('msg')", "error aborts execution and reports message string."},
                        {"warning", "Display warning message.", "warning('msg')", "warning outputs diagnostic warning without halting execution."},
                        {"lastwarn", "Last warning message and identifier.", "[MSG, ID] = lastwarn", "lastwarn retrieves or resets the most recent warning."},
                        {"assert", "Generate error if condition is false.", "assert(COND, 'msg')", "assert throws error if logical condition evaluates to false."}
                    }
                },
                {
                    "Workspace and session management.",
                    {
                        {"clear", "Clear variables, functions, and session items from memory.", "clear or clear VAR1 VAR2", "clear removes specified items or all variables from workspace."},
                        {"clc", "Clear command window.", "clc", "clc clears the interactive terminal output."},
                        {"who", "List current variables in workspace.", "who", "who displays variable names in workspace."},
                        {"whos", "List current variables with size, bytes, and class.", "whos", "whos displays detailed table of workspace variables."},
                        {"which", "Locate functions and files.", "which NAME", "which displays origin (built-in, function, .m file) of NAME."},
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

        // ── 12. iofun: File input and output ─────────────────────────────
        {
            "iofun",
            "File input and output, workspace persistence.",
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
                },
                {
                    "Workspace persistence.",
                    {
                        {"save", "Save workspace variables to file.", "save(FILENAME, VAR1, VAR2)", "save stores workspace variables in binary .MAT or ASCII formats."},
                        {"load", "Load variables from file into workspace.", "load(FILENAME, VAR1, ...)", "load reads variables from .MAT file into current workspace."}
                    }
                }
            }
        },

        // ── 13. signal: Signal Processing Toolbox ────────────────────────
        {
            "signal",
            "Signal Processing Toolbox.",
            {
                {
                    "Filter design and implementation.",
                    {
                        {"butter", "Butterworth filter design.", "[B, A] = butter(N, Wn, BTYPE)", "butter designs Nth-order digital or analog Butterworth filter."},
                        {"cheby1", "Chebyshev Type I filter design.", "[B, A] = cheby1(N, Rp, Wn)", "cheby1 designs Chebyshev Type I filter with passband ripple."},
                        {"cheby2", "Chebyshev Type II filter design.", "[B, A] = cheby2(N, Rs, Wn)", "cheby2 designs Chebyshev Type II filter with stopband attenuation."},
                        {"ellip", "Elliptic (Cauer) filter design.", "[B, A] = ellip(N, Rp, Rs, Wn)", "ellip designs elliptic filter with equiripple passband and stopband."},
                        {"fir1", "Window-based FIR filter design.", "B = fir1(N, Wn, TYPE, WIN)", "fir1 designs linear-phase FIR filter using window method."},
                        {"fir2", "Frequency-sampling FIR filter design.", "B = fir2(N, F, A)", "fir2 designs arbitrary shape FIR filter using frequency sampling."},
                        {"firpm", "Parks-McClellan optimal equiripple FIR filter.", "B = firpm(N, F, A)", "firpm designs optimal equiripple linear-phase FIR filter."},
                        {"filtfilt", "Zero-phase forward and reverse digital filtering.", "Y = filtfilt(B, A, X)", "filtfilt performs forward and backward filtering with zero phase distortion."},
                        {"filter", "1-D digital rational transfer function filter.", "Y = filter(B, A, X)", "filter implements Direct Form II Transposed difference equations."},
                        {"sosfilt", "Second-order sections (biquad) filtering.", "Y = sosfilt(SOS, X)", "sosfilt evaluates cascaded second-order sections filter."},
                        {"tf2sos", "Transfer function to second-order sections.", "[SOS, G] = tf2sos(B, A)", "tf2sos decomposes high-order rational filter into stable biquads."},
                        {"sos2tf", "Second-order sections to transfer function.", "[B, A] = sos2tf(SOS, G)", "sos2tf converts cascaded biquad representation back to polynomial ratio."}
                    }
                },
                {
                    "Spectral analysis and transforms.",
                    {
                        {"periodogram", "Periodogram spectral power estimate.", "[Pxx, W] = periodogram(X, WIN, NFFT, Fs)", "periodogram computes discrete power spectral density estimate."},
                        {"pwelch", "Welch's averaged periodogram spectral estimate.", "[Pxx, W] = pwelch(X, WIN, NOVERLAP, NFFT, Fs)", "pwelch computes low-variance power spectrum via overlapping segments."},
                        {"spectrogram", "Short-Time Fourier Transform spectrogram.", "[S, F, T, P] = spectrogram(X, WIN, NOVERLAP, NFFT, Fs)", "spectrogram computes time-frequency spectrogram distribution."},
                        {"hilbert", "Hilbert transform and analytic signal.", "Z = hilbert(X)", "hilbert creates discrete-time analytic signal Z = X + i*H(X)."},
                        {"dct", "Discrete Cosine Transform (Type II).", "Y = dct(X)", "dct evaluates orthonormal Discrete Cosine Transform."},
                        {"idct", "Inverse Discrete Cosine Transform.", "Y = idct(X)", "idct reconstructs original signal from DCT coefficients."},
                        {"czt", "Chirp Z-transform.", "Y = czt(X, K, W, A)", "czt evaluates Z-transform along spiral contours in complex plane."}
                    }
                }
            }
        },

        // ── 14. stats: Statistics and Machine Learning ───────────────────
        {
            "stats",
            "Statistics and Machine Learning Toolbox.",
            {
                {
                    "Probability distributions.",
                    {
                        {"normpdf", "Normal probability density function.", "Y = normpdf(X, MU, SIGMA)", "normpdf evaluates Gaussian normal probability density."},
                        {"normcdf", "Normal cumulative distribution function.", "P = normcdf(X, MU, SIGMA)", "normcdf computes standard normal cumulative distribution integral."},
                        {"norminv", "Normal inverse cumulative distribution.", "X = norminv(P, MU, SIGMA)", "norminv computes inverse Gaussian quantile function."},
                        {"normfit", "Normal distribution parameter estimation.", "[MUHAT, SIGMAHAT] = normfit(X)", "normfit computes maximum likelihood estimates of mean and variance."},
                        {"tpdf", "Student's t probability density function.", "Y = tpdf(X, V)", "tpdf evaluates Student's t distribution with V degrees of freedom."},
                        {"tcdf", "Student's t cumulative distribution function.", "P = tcdf(X, V)", "tcdf evaluates Student's t cumulative distribution."},
                        {"tinv", "Student's t inverse cumulative distribution.", "X = tinv(P, V)", "tinv computes Student's t distribution quantiles."},
                        {"chi2pdf", "Chi-square probability density function.", "Y = chi2pdf(X, V)", "chi2pdf evaluates chi-square probability density."},
                        {"chi2cdf", "Chi-square cumulative distribution function.", "P = chi2cdf(X, V)", "chi2cdf evaluates chi-square cumulative distribution."},
                        {"chi2inv", "Chi-square inverse cumulative distribution.", "X = chi2inv(P, V)", "chi2inv evaluates chi-square quantiles."},
                        {"fpdf", "F probability density function.", "Y = fpdf(X, V1, V2)", "fpdf evaluates Snedecor's F distribution density."},
                        {"fcdf", "F cumulative distribution function.", "P = fcdf(X, V1, V2)", "fcdf evaluates F cumulative distribution integral."},
                        {"finv", "F inverse cumulative distribution.", "X = finv(P, V1, V2)", "finv evaluates F distribution quantiles."},
                        {"poisspdf", "Poisson probability mass function.", "Y = poisspdf(X, LAMBDA)", "poisspdf computes Poisson discrete probability P(X=k)."},
                        {"poisscdf", "Poisson cumulative distribution function.", "P = poisscdf(X, LAMBDA)", "poisscdf evaluates Poisson cumulative distribution."},
                        {"binopdf", "Binomial probability mass function.", "Y = binopdf(X, N, P)", "binopdf computes binomial discrete probability mass."},
                        {"binocdf", "Binomial cumulative distribution function.", "Y = binocdf(X, N, P)", "binocdf computes binomial cumulative distribution."}
                    }
                },
                {
                    "Hypothesis testing, regression and clustering.",
                    {
                        {"ttest", "One-sample or paired-sample t-test.", "[H, P, CI, STATS] = ttest(X, M)", "ttest performs Student's t-test of null hypothesis mean(X) == M."},
                        {"ttest2", "Two-sample independent t-test.", "[H, P, CI, STATS] = ttest2(X, Y)", "ttest2 tests if two independent samples have equal means."},
                        {"ztest", "Z-test for known population variance.", "[H, P, CI, STATS] = ztest(X, M, SIGMA)", "ztest performs normal Z-test of sample mean."},
                        {"anova1", "One-way analysis of variance (ANOVA).", "[P, TABLE, STATS] = anova1(X, GROUP)", "anova1 tests whether group columns have identical means."},
                        {"regress", "Multiple linear regression least-squares fit.", "[B, BINT, R, RINT, STATS] = regress(Y, X)", "regress computes multivariate linear least squares regression."},
                        {"lasso", "Lasso and elastic net linear regression.", "[B, FITINFO] = lasso(X, Y)", "lasso fits regularized linear regression models with L1 penalty."},
                        {"pca", "Principal Component Analysis.", "[COEFF, SCORE, LATENT, TSQUARED, EXPLAINED] = pca(X)", "pca computes principal component loadings and variances."},
                        {"kmeans", "K-means clustering.", "[IDX, C, SUMD, D] = kmeans(X, K)", "kmeans partitions observations into K clusters by minimizing squared Euclidean distance."}
                    }
                }
            }
        },

        // ── 15. image: Image Processing Toolbox ──────────────────────────
        {
            "image",
            "Image Processing Toolbox.",
            {
                {
                    "Spatial filtering, morphology and color conversions.",
                    {
                        {"rgb2gray", "Convert RGB image or colormap to grayscale.", "I = rgb2gray(RGB)", "rgb2gray converts RGB color image to luminance intensity map."},
                        {"gray2rgb", "Convert grayscale intensity image to RGB.", "RGB = gray2rgb(I)", "gray2rgb replicates grayscale image into 3 color channels."},
                        {"rgb2hsv", "Convert RGB to HSV color space.", "HSV = rgb2hsv(RGB)", "rgb2hsv converts red-green-blue components to Hue-Saturation-Value."},
                        {"hsv2rgb", "Convert HSV to RGB color space.", "RGB = hsv2rgb(HSV)", "hsv2rgb converts Hue-Saturation-Value to red-green-blue."},
                        {"imfilter", "N-D spatial multidimensional image filtering.", "J = imfilter(I, H, BOUNDARY)", "imfilter computes discrete 2-D spatial correlation or convolution."},
                        {"fspecial", "Create predefined 2-D spatial filters.", "H = fspecial(TYPE, PARAMS)", "fspecial creates standard filters ('gaussian', 'sobel', 'laplacian')."},
                        {"imerode", "Erode grayscale or binary image.", "IM2 = imerode(IM, SE)", "imerode computes morphological erosion using structuring element SE."},
                        {"imdilate", "Dilate grayscale or binary image.", "IM2 = imdilate(IM, SE)", "imdilate computes morphological dilation using structuring element SE."},
                        {"imopen", "Morphologically open image (erosion then dilation).", "IM2 = imopen(IM, SE)", "imopen removes small bright foreground artifacts."},
                        {"imclose", "Morphologically close image (dilation then erosion).", "IM2 = imclose(IM, SE)", "imclose closes small dark holes in foreground objects."},
                        {"imreconstruct", "Morphological grayscale reconstruction by dilation.", "IM2 = imreconstruct(MARKER, MASK)", "imreconstruct performs geodesic morphological reconstruction."},
                        {"bwlabel", "Label connected components in 2-D binary image.", "[L, NUM] = bwlabel(BW, CONN)", "bwlabel assigns integer labels to connected components (4- or 8-connected)."},
                        {"imresize", "Resize image using interpolation.", "J = imresize(I, SCALE, METHOD)", "imresize resamples image dimensions via bicubic or bilinear interpolation."},
                        {"imrotate", "Rotate image by angle.", "J = imrotate(I, ANGLE, METHOD)", "imrotate rotates 2-D image around center by specified degrees."},
                        {"psnr", "Peak Signal-to-Noise Ratio for images.", "[P, MSE] = psnr(A, REF)", "psnr computes Peak Signal-to-Noise Ratio between images in decibels."},
                        {"ssim", "Structural Similarity Index for images.", "[SSIMVAL, SSIMMAP] = ssim(A, REF)", "ssim measures structural degradation between distorted and reference images."}
                    }
                }
            }
        },

        // ── 16. control: Control System Toolbox ──────────────────────────
        {
            "control",
            "Control System Toolbox.",
            {
                {
                    "LTI model creation and interconnection.",
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
        },

        // ── 17. optim: Optimization Toolbox ──────────────────────────────
        {
            "optim",
            "Optimization Toolbox.",
            {
                {
                    "Nonlinear optimization and root finding.",
                    {
                        {"fminunc", "Find minimum of unconstrained multivariable function.", "[X, FVAL, EXITFLAG] = fminunc(@fun, X0)", "fminunc minimizes objective function using BFGS quasi-Newton method."},
                        {"fminsearch", "Unconstrained multivariable minimization using Nelder-Mead simplex.", "[X, FVAL] = fminsearch(@fun, X0)", "fminsearch minimizes derivative-free function using Nelder-Mead simplex."},
                        {"fminbnd", "Find minimum of single-variable function on bounded interval.", "[X, FVAL] = fminbnd(@fun, X1, X2)", "fminbnd performs golden section search with parabolic interpolation."},
                        {"fzero", "Root of single-variable nonlinear continuous function.", "[X, FVAL] = fzero(@fun, X0)", "fzero finds zero of continuous scalar function using Brent's method."},
                        {"fsolve", "Solve system of nonlinear equations F(x) = 0.", "[X, FVAL] = fsolve(@fun, X0)", "fsolve finds root vector of nonlinear vector equations using Trust-Region Dogleg."},
                        {"lsqnonlin", "Solve nonlinear least-squares curve-fitting problems.", "[X, RESNORM] = lsqnonlin(@fun, X0)", "lsqnonlin minimizes sum of squares sum(f_i(x)^2) using Levenberg-Marquardt."}
                    }
                }
            }
        },

        // ── 18. wavelet: Wavelet Toolbox ─────────────────────────────────
        {
            "wavelet",
            "Wavelet Toolbox.",
            {
                {
                    "1-D and 2-D wavelet transforms.",
                    {
                        {"dwt", "Single-level 1-D Discrete Wavelet Transform.", "[CA, CD] = dwt(X, 'wname')", "dwt computes single-level approximation CA and detail CD coefficients."},
                        {"idwt", "Single-level 1-D Inverse Discrete Wavelet Transform.", "X = idwt(CA, CD, 'wname')", "idwt reconstructs 1-D signal from approximation and detail coefficients."},
                        {"dwt2", "Single-level 2-D Discrete Wavelet Transform.", "[CA, CH, CV, CD] = dwt2(X, 'wname')", "dwt2 computes 2-D image decomposition into approximation and directional details."},
                        {"idwt2", "Single-level 2-D Inverse Discrete Wavelet Transform.", "X = idwt2(CA, CH, CV, CD, 'wname')", "idwt2 reconstructs 2-D image from approximation and detail matrices."},
                        {"wavedec", "Multilevel 1-D wavelet decomposition.", "[C, L] = wavedec(X, N, 'wname')", "wavedec computes N-level wavelet decomposition vector C and bookkeeping vector L."},
                        {"waverec", "Multilevel 1-D wavelet reconstruction.", "X = waverec(C, L, 'wname')", "waverec reconstructs 1-D signal from multilevel decomposition components."},
                        {"wavedec2", "Multilevel 2-D wavelet decomposition.", "[C, S] = wavedec2(X, N, 'wname')", "wavedec2 computes N-level 2-D wavelet decomposition coefficients."},
                        {"waverec2", "Multilevel 2-D wavelet reconstruction.", "X = waverec2(C, S, 'wname')", "waverec2 reconstructs 2-D image from multilevel coefficients."},
                        {"swt", "Stationary Wavelet Transform (undecimated).", "[SWA, SWD] = swt(X, N, 'wname')", "swt computes translation-invariant undecimated wavelet transform."},
                        {"iswt", "Inverse Stationary Wavelet Transform.", "X = iswt(SWA, SWD, 'wname')", "iswt reconstructs signal from stationary wavelet coefficients."}
                    }
                },
                {
                    "Wavelet filters and denoising.",
                    {
                        {"wfilters", "Wavelet decomposition and reconstruction filters.", "[LoD, HiD, LoR, HiR] = wfilters('wname')", "wfilters returns lowpass and highpass FIR filter coefficients."},
                        {"waveinfo", "Information on wavelet families.", "waveinfo('wname')", "waveinfo displays properties, vanishing moments, and filter lengths of wavelet families."},
                        {"wden", "Automatic 1-D wavelet denoising.", "[XD, CXD, LXD] = wden(X, TPTR, SORH, SCAL, N, 'wname')", "wden performs thresholding denoising on 1-D signal."},
                        {"thselect", "Threshold selection for denoising.", "THR = thselect(X, 'rigrsure')", "thselect computes universal (sqtwolog) or Stein's Unbiased Risk threshold."},
                        {"wthresh", "Soft or hard thresholding.", "Y = wthresh(X, 's', T)", "wthresh applies soft or hard thresholding to input coefficients."},
                        {"wenergy", "Energy percentage of wavelet decomposition.", "[Ea, Ed] = wenergy(C, L)", "wenergy computes percentage of energy stored in approximation and details."}
                    }
                }
            }
        },

        // ── 19. comm: Communications Toolbox ─────────────────────────────
        {
            "comm",
            "Communications Toolbox.",
            {
                {
                    "Digital modulation and demodulation.",
                    {
                        {"pskmod", "Phase Shift Keying modulation.", "Y = pskmod(X, M, PHI)", "pskmod maps integer symbols 0..M-1 to M-ary PSK constellation."},
                        {"pskdemod", "Phase Shift Keying demodulation.", "Z = pskdemod(Y, M, PHI)", "pskdemod recovers symbols from received complex M-ary PSK samples."},
                        {"qammod", "Quadrature Amplitude Modulation.", "Y = qammod(X, M, OPTS)", "qammod maps integer symbols to complex square or rectangular M-QAM constellation."},
                        {"qamdemod", "Quadrature Amplitude Demodulation.", "Z = qamdemod(Y, M, OPTS)", "qamdemod demodulates complex M-QAM constellation points back to integer symbols."},
                        {"fskmod", "Frequency Shift Keying modulation.", "Y = fskmod(X, M, FREQ_SEP, NSAMP, Fs)", "fskmod generates discrete-time M-ary FSK modulated waveform."},
                        {"fskdemod", "Frequency Shift Keying demodulation.", "Z = fskdemod(Y, M, FREQ_SEP, NSAMP, Fs)", "fskdemod recovers integer symbols from M-FSK waveform."},
                        {"pammod", "Pulse Amplitude Modulation.", "Y = pammod(X, M, PHI)", "pammod modulates integer symbols into real 1-D M-PAM signal levels."},
                        {"pamdemod", "Pulse Amplitude Demodulation.", "Z = pamdemod(Y, M, PHI)", "pamdemod demodulates M-PAM signal levels back to integer symbols."}
                    }
                },
                {
                    "Channel modeling and performance metrics.",
                    {
                        {"awgn", "Add white Gaussian noise to signal.", "Y = awgn(X, SNR, 'measured')", "awgn adds white Gaussian noise of specified Signal-to-Noise Ratio (dB)."},
                        {"biterr", "Compute number of bit errors and Bit Error Rate.", "[NUM, RATIO] = biterr(A, B)", "biterr compares bit matrices and returns total error count and BER."},
                        {"symerr", "Compute symbol errors and Symbol Error Rate.", "[NUM, RATIO] = symerr(A, B)", "symerr counts differing symbol elements and calculates SER."},
                        {"berawgn", "Theoretical Bit Error Rate for AWGN channel.", "BER = berawgn(EbNo, 'psk', M, 'nondiff')", "berawgn calculates theoretical closed-form BER/SER for standard constellations."}
                    }
                }
            }
        },

        // ── 20. audio: Audio Toolbox ─────────────────────────────────────
        {
            "audio",
            "Audio Toolbox.",
            {
                {
                    "Audio file input and output.",
                    {
                        {"audioread", "Read audio file.", "[Y, FS] = audioread(FILENAME)", "audioread loads PCM/WAV/FLAC audio samples normalized in [-1, 1] and sampling rate FS."},
                        {"audiowrite", "Write audio file.", "audiowrite(FILENAME, Y, FS)", "audiowrite exports audio matrix Y at sampling rate FS to audio file."},
                        {"audioinfo", "Information about audio file.", "INFO = audioinfo(FILENAME)", "audioinfo returns struct with duration, sample rate, channels, and bits per sample."}
                    }
                },
                {
                    "Acoustic and spectral analysis.",
                    {
                        {"pitch", "Fundamental frequency (F0 pitch) of audio signal.", "[F0, LOCS] = pitch(AUDIO, FS)", "pitch estimates time-varying fundamental frequency contour using NDF/PEF methods."},
                        {"mfcc", "Mel-Frequency Cepstral Coefficients.", "[COEFFS, DELTA] = mfcc(AUDIO, FS)", "mfcc computes 13-band acoustic Mel-frequency cepstral feature representations."},
                        {"spectralCentroid", "Spectral centroid frequency.", "C = spectralCentroid(AUDIO, FS)", "spectralCentroid computes frequency center of mass of power spectrum."},
                        {"spectralSpread", "Spectral spread (spectral standard deviation).", "S = spectralSpread(AUDIO, FS)", "spectralSpread computes second central moment of spectrum around centroid."},
                        {"spectralFlux", "Spectral flux (rate of spectral change).", "F = spectralFlux(AUDIO, FS)", "spectralFlux measures rate of local spectral power frame changes over time."},
                        {"spectralRolloff", "Spectral roll-off frequency point.", "R = spectralRolloff(AUDIO, FS, THRESH)", "spectralRolloff computes frequency below which 85% or 95% of spectral energy lies."}
                    }
                }
            }
        },

        // ── 21. ode: Ordinary Differential Equations ─────────────────────
        {
            "ode",
            "Ordinary Differential Equation (ODE) Solvers.",
            {
                {
                    "Initial value problem numerical solvers.",
                    {
                        {"ode45", "Solve non-stiff differential equations (Dormand-Prince 4(5)).", "[T, Y] = ode45(@odefun, TSPAN, Y0, OPTS)", "ode45 is the standard explicit adaptive Runge-Kutta 4th/5th order solver for non-stiff ODEs."},
                        {"ode23", "Solve non-stiff differential equations (Bogacki-Shampine 2(3)).", "[T, Y] = ode23(@odefun, TSPAN, Y0, OPTS)", "ode23 is a lower-order adaptive Runge-Kutta solver suitable for moderate error tolerances."},
                        {"ode113", "Solve non-stiff differential equations (variable-order Adams-Bashforth-Moulton).", "[T, Y] = ode113(@odefun, TSPAN, Y0, OPTS)", "ode113 is a multi-step variable-order Adams predictor-corrector solver for smooth problems."},
                        {"ode15s", "Solve stiff differential equations and DAEs (variable-order BDF/NDFs).", "[T, Y] = ode15s(@odefun, TSPAN, Y0, OPTS)", "ode15s is an implicit multi-step numerical differentiation formula solver for stiff systems."},
                        {"ode23s", "Solve stiff differential equations (modified Rosenbrock order 2).", "[T, Y] = ode23s(@odefun, TSPAN, Y0, OPTS)", "ode23s is a one-step implicit Rosenbrock solver effective for crude-tolerance stiff ODEs."},
                        {"odeset", "Create or alter ODE options structure.", "OPTS = odeset('RelTol', 1e-4, 'AbsTol', 1e-6)", "odeset configures tolerances, Jacobian functions, event functions, and max step size."},
                        {"odeget", "Extract ODE options parameter.", "VAL = odeget(OPTS, 'RelTol')", "odeget retrieves parameter value from ODE options structure."}
                    }
                }
            }
        },

        // ── 22. graphics: Plotting and visualization ─────────────────────
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

} // namespace numkit::bundle
