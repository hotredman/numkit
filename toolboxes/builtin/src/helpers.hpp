#pragma once
// helpers.hpp moved to ops/ (numkit_ops) as step C0 of the builtin -> math+lang
// split: it is an engine-free value+ops helper substrate shared by BOTH the
// math/ and language/ sub-trees (~60 in-tree includers), so it must live below
// both before they separate into the `math` and `lang` libs. This forwarding
// stub keeps the existing `#include "helpers.hpp"` call-sites compiling during
// the migration; they retarget to <numkit/ops/helpers.hpp> as their TUs
// relocate (C1/C2) or in the C4 cleanup pass, after which this stub is removed.
#include <numkit/ops/helpers.hpp>
