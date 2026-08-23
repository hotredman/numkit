/// @file struct.hpp
/// @ingroup group_datatypes
// toolboxes/builtin/include/numkit/builtin/language/structures/struct.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/fn_handle.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit {

/// @addtogroup group_datatypes
/// @{

class Engine;
}

namespace numkit::runtime {

void registerStructuresRuntime(Engine &engine);
void registerStructfunCallbackBuiltin(Engine &engine);

/// @brief Empty struct scalar (`s = struct()`).
///
/// `struct()` (no args) returns a 1×1 scalar struct
/// with no fields. C++ name is `structure` because `struct` is a keyword
/// (registered name remains `struct`).
///
/// @param mr  Memory resource (nullptr → process default).
/// @return    Empty scalar struct.
/// @see structure(nameValuePairs, mr)
Value structure(std::pmr::memory_resource *mr = nullptr);

/// @brief Struct from `{name, value, …}` pairs
/// (`s = struct(name1, val1, name2, val2, …)`).
///
/// Odd arg count silently drops the trailing unmatched name. Non-char
/// names throw.
///
/// @param nameValuePairs  Alternating name (CHAR / STRING) and value pairs.
/// @param mr              Memory resource (nullptr → process default).
/// @return                Scalar struct with the requested fields.
/// @throws Error  Non-char name in the pair list
///                (`m:struct:badName`).
Value structure(Span<const Value> nameValuePairs,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Field names of a struct (`fn = fieldnames(s)`).
///
/// @param s   Struct Value.
/// @param mr  Memory resource (nullptr → process default).
/// @return    1-column cell array of field names in insertion order.
/// @throws Error  `s` is not a struct (`m:fieldnames:notStruct`).
Value fieldnames(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Test for field existence (`tf = isfield(s, name)`).
///
/// Returns `false` (not error) if `s` is not a struct.
///
/// @param s     Struct (or any Value).
/// @param name  Field name (CHAR / STRING) or cell array of names —
///              broadcasts elementwise over cell entries.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL result: scalar (single name) or array (cell name).
Value isfield(const Value &s, const Value &name,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Remove a field from a struct (`s2 = rmfield(s, name)`).
///
/// Silently ignores missing field names.
///
/// @param s     Struct Value.
/// @param name  Field name (or cell array of names) to remove.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Copy of `s` with the requested fields removed.
/// @throws Error  `s` is not a struct (`m:rmfield:notStruct`).
Value rmfield(const Value &s, const Value &name,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Apply a function handle to every field of a struct
/// (`y = structfun(fn, s, uniformOutput)`).
///
/// Built-in fast-path set matches @ref cellfun.
///
/// The callback is invoked once per field with a 1-element `args`
/// holding the field value and writes a single Value into `outs[0]`.
///
/// @param fn             Callback.
/// @param s              Struct Value.
/// @param uniformOutput  `true` → numeric column vector of length
///                       `numFields` (type from first result);
///                       `false` → `numFields × 1` cell column.
/// @param mr             Memory resource (nullptr → process default).
/// @return               Per-field results (shape depends on
///                       `uniformOutput`).
/// @throws Error         Non-struct `s`, or non-scalar result in
Value structfun(FnHandle fn, const Value &s, bool uniformOutput,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Gets the value of specified field in structure (`v = getfield(s, 'field')`).
/// @param s Structure array.
/// @param name Field name string.
/// @param mr Memory resource for output allocation.
/// @return Field value.
/// @see setfield, rmfield, isfield
Value getfield(const Value &s, const Value &name, std::pmr::memory_resource *mr = nullptr);

/// @brief Sets the value of specified field in structure (`s = setfield(s, 'field', val)`).
/// @param s Structure array.
/// @param name Field name string.
/// @param value New field value.
/// @param mr Memory resource for output allocation.
/// @return Updated structure array.
/// @see getfield, rmfield, isfield
Value setfield(const Value &s, const Value &name, const Value &value, std::pmr::memory_resource *mr = nullptr);

/// @brief Orders fields of structure array alphabetically (`s = orderfields(s)`).
/// @param s Structure array.
/// @param mr Memory resource for output allocation.
/// @return Structure array with sorted field names.
/// @see fieldnames, isfield
Value orderfields(const Value &s, std::pmr::memory_resource *mr = nullptr);

/// @brief Converts structure array to cell array of field values (`c = struct2cell(s)`).
/// @param s Structure array.
/// @param mr Memory resource for output allocation.
/// @return Cell array containing all field contents.
/// @see cell2struct, structfun
Value struct2cell(const Value &s, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::runtime

