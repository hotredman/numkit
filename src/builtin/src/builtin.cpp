#include <numkit/builtin/builtin.hpp>
#include <numkit/builtin/specfun.hpp>
#include <numkit/builtin/polyfun.hpp>
#include <numkit/builtin/strfun.hpp>
#include <numkit/builtin/timefun.hpp>
#include <numkit/builtin/datatypes.hpp>
#include <numkit/builtin/iofun.hpp>
#include <numkit/builtin/general.hpp>
#include <numkit/builtin/lang.hpp>

namespace numkit::bundle::builtin {
void register_ops(Engine &engine);
void register_elfun(Engine &engine);
void register_elmat(Engine &engine);
void register_matfun(Engine &engine);
void register_datafun(Engine &engine);
}

namespace numkit {

void BuiltinLibrary::install(Engine &engine) {
    bundle::builtin::register_ops(engine);
    bundle::builtin::register_elfun(engine);
    bundle::builtin::register_elmat(engine);
    bundle::builtin::register_matfun(engine);
    bundle::builtin::register_datafun(engine);
    builtin::register_specfun(engine);
    builtin::register_polyfun(engine);
    builtin::register_strfun(engine);
    builtin::register_timefun(engine);
    builtin::register_datatypes(engine);
    builtin::register_iofun(engine);
    builtin::register_general(engine);
    builtin::register_lang(engine);
}

void BuiltinLibrary::registerWorkspaceBuiltins(Engine &) {
    // Handled modularly via register_general, register_datatypes, register_timefun, etc.
}

} // namespace numkit
