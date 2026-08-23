// src/builtin/src/datatypes/predicates.cpp
//
// Data type predicates and introspection implementations for numkit::builtin.

#include <numkit/builtin/datatypes.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/types/types.hpp>

namespace numkit::builtin {

Value iscell(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::iscell(v, mr); }
Value isstruct(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isstruct(v, mr); }
Value isnumeric(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isnumeric(v, mr); }
Value isfloat(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isfloat(v, mr); }
Value isinteger(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isinteger(v, mr); }
Value islogical(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::islogical(v, mr); }
Value ischar(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::ischar(v, mr); }
Value isstring(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isstring(v, mr); }
Value isnan(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isnan(v, mr); }
Value isinf(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isinf(v, mr); }
Value isfinite(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isfinite(v, mr); }
Value isempty(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isempty(v, mr); }
Value isscalar(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isscalar(v, mr); }
Value isvector(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isvector(v, mr); }
Value isrow(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::isrow(v, mr); }
Value iscolumn(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::iscolumn(v, mr); }
Value ismatrix(const Value &v, std::pmr::memory_resource *mr) { return numkit::lang::ismatrix(v, mr); }
Value isequal(const Value &a, const Value &b, std::pmr::memory_resource *mr) { return numkit::lang::isequal(a, b, mr); }
Value isequaln(const Value &a, const Value &b, std::pmr::memory_resource *mr) { return numkit::lang::isequaln(a, b, mr); }

} // namespace numkit::builtin
