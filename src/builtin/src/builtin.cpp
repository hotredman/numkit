#include <numkit/builtin/builtin.hpp>
#include <numkit/builtin/general.hpp>
#include <numkit/builtin/lang.hpp>

namespace numkit::bundle::builtin {
void register_ops(Engine &engine);
void register_elfun(Engine &engine);
void register_elmat(Engine &engine);
void register_matfun(Engine &engine);
void register_datafun(Engine &engine);
void register_specfun(Engine &engine);
void register_polyfun(Engine &engine);
void register_strfun(Engine &engine);
void register_timefun(Engine &engine);
void register_datatypes(Engine &engine);
void register_iofun(Engine &engine);
}

namespace numkit {

void BuiltinLibrary::install(Engine &engine) {
    bundle::builtin::register_ops(engine);
    bundle::builtin::register_elfun(engine);
    bundle::builtin::register_elmat(engine);
    bundle::builtin::register_matfun(engine);
    bundle::builtin::register_datafun(engine);
    bundle::builtin::register_specfun(engine);
    bundle::builtin::register_polyfun(engine);
    bundle::builtin::register_strfun(engine);
    bundle::builtin::register_timefun(engine);
    bundle::builtin::register_datatypes(engine);
    bundle::builtin::register_iofun(engine);
    builtin::register_general(engine);
    builtin::register_lang(engine);
}

void BuiltinLibrary::registerWorkspaceBuiltins(Engine &) {
    // Handled modularly via register_general, register_datatypes, register_timefun, etc.
}

} // namespace numkit
