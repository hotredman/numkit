// src/bundle/src/register/builtin/datafun_reg.cpp

#include <numkit/builtin/datafun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <stdexcept>

namespace numkit::builtin::detail {

void accumarray_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void allunique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitcmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitget_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitset_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitshift_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void boundary_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void colperm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convhull_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void deg2rad_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void del2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void delaunay_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void discretize_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expm1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void findgroups_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gradient_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void griddata_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void griddatan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupfilter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupsummary_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void grouptransform_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void inpolygon_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intersect_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismember_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismembertol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void linspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log1p_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void matchpairs_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void max_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mean_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void min_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nthroot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void numunique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyarea_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pow2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void prod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rad2deg_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randperm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reallog_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realpow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realsqrt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rng_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setdiff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void splitapply_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void symrcm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void union_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void unique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uniquetol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo180_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo2Pi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo360_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapToPi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_datafun(Engine &engine) {
    engine.registerFunction("linspace", &::numkit::builtin::detail::linspace_reg);
    engine.registerFunction("logspace", &::numkit::builtin::detail::logspace_reg);
    engine.registerFunction("rand",     &::numkit::builtin::detail::rand_reg);
    engine.registerFunction("randn",    &::numkit::builtin::detail::randn_reg);
    engine.registerFunction("randi",    &::numkit::builtin::detail::randi_reg);
    engine.registerFunction("randperm", &::numkit::builtin::detail::randperm_reg);
    engine.registerFunction("rng",      &::numkit::builtin::detail::rng_reg);

    engine.registerFunction("max",      &::numkit::builtin::detail::max_reg);
    engine.registerFunction("min",      &::numkit::builtin::detail::min_reg);
    engine.registerFunction("sum",      &::numkit::builtin::detail::sum_reg);
    engine.registerFunction("prod",     &::numkit::builtin::detail::prod_reg);
    engine.registerFunction("mean",     &::numkit::builtin::detail::mean_reg);

    engine.registerFunction("nthroot",  &::numkit::builtin::detail::nthroot_reg);
    engine.registerFunction("expm1",    &::numkit::builtin::detail::expm1_reg);
    engine.registerFunction("log1p",    &::numkit::builtin::detail::log1p_reg);
    engine.registerFunction("pow2",     &::numkit::builtin::detail::pow2_reg);
    engine.registerFunction("realpow",  &::numkit::builtin::detail::realpow_reg);
    engine.registerFunction("reallog",  &::numkit::builtin::detail::reallog_reg);
    engine.registerFunction("realsqrt", &::numkit::builtin::detail::realsqrt_reg);

    engine.registerFunction("bitand",   &::numkit::builtin::detail::bitand_reg);
    engine.registerFunction("bitor",    &::numkit::builtin::detail::bitor_reg);
    engine.registerFunction("bitxor",   &::numkit::builtin::detail::bitxor_reg);
    engine.registerFunction("bitshift", &::numkit::builtin::detail::bitshift_reg);
    engine.registerFunction("bitcmp",   &::numkit::builtin::detail::bitcmp_reg);
    engine.registerFunction("bitset",   &::numkit::builtin::detail::bitset_reg);
    engine.registerFunction("bitget",   &::numkit::builtin::detail::bitget_reg);

    engine.registerFunction("unique",     &::numkit::builtin::detail::unique_reg);
    engine.registerFunction("ismember",   &::numkit::builtin::detail::ismember_reg);
    engine.registerFunction("union",      &::numkit::builtin::detail::union_reg);
    engine.registerFunction("intersect",  &::numkit::builtin::detail::intersect_reg);
    engine.registerFunction("setdiff",    &::numkit::builtin::detail::setdiff_reg);
    engine.registerFunction("setxor",     &::numkit::builtin::detail::setxor_reg);
    engine.registerFunction("allunique",  &::numkit::builtin::detail::allunique_reg);
    engine.registerFunction("numunique",  &::numkit::builtin::detail::numunique_reg);
    engine.registerFunction("ismembertol",&::numkit::builtin::detail::ismembertol_reg);
    engine.registerFunction("uniquetol",  &::numkit::builtin::detail::uniquetol_reg);
    engine.registerFunction("histcounts", &::numkit::builtin::detail::histcounts_reg);
    engine.registerFunction("histc",      &::numkit::builtin::detail::histc_reg);
    engine.registerFunction("discretize", &::numkit::builtin::detail::discretize_reg);
    engine.registerFunction("accumarray", &::numkit::builtin::detail::accumarray_reg);
    engine.registerFunction("deg2rad",    &::numkit::builtin::detail::deg2rad_reg);
    engine.registerFunction("rad2deg",    &::numkit::builtin::detail::rad2deg_reg);
    engine.registerFunction("wrapToPi",   &::numkit::builtin::detail::wrapToPi_reg);
    engine.registerFunction("wrapTo2Pi",  &::numkit::builtin::detail::wrapTo2Pi_reg);
    engine.registerFunction("wrapTo180",  &::numkit::builtin::detail::wrapTo180_reg);
    engine.registerFunction("wrapTo360",  &::numkit::builtin::detail::wrapTo360_reg);

    engine.registerFunction("gradient",   &::numkit::builtin::detail::gradient_reg);
    engine.registerFunction("del2",       &::numkit::builtin::detail::del2_reg);
    engine.registerFunction("inpolygon",  &::numkit::builtin::detail::inpolygon_reg);
    engine.registerFunction("convhull",   &::numkit::builtin::detail::convhull_reg);
    engine.registerFunction("polyarea",   &::numkit::builtin::detail::polyarea_reg);
    engine.registerFunction("boundary",   &::numkit::builtin::detail::boundary_reg);
    engine.registerFunction("delaunay",   &::numkit::builtin::detail::delaunay_reg);
    engine.registerFunction("histcounts2",&::numkit::builtin::detail::histcounts2_reg);
    engine.registerFunction("griddata",   &::numkit::builtin::detail::griddata_reg);
    engine.registerFunction("griddatan",  &::numkit::builtin::detail::griddatan_reg);
    engine.registerFunction("matchpairs", &::numkit::builtin::detail::matchpairs_reg);
    engine.registerFunction("findgroups", &::numkit::builtin::detail::findgroups_reg);
    engine.registerFunction("splitapply", &::numkit::builtin::detail::splitapply_reg);
    engine.registerFunction("groupcounts",&::numkit::builtin::detail::groupcounts_reg);
    engine.registerFunction("groupsummary",&::numkit::builtin::detail::groupsummary_reg);
    engine.registerFunction("grouptransform",&::numkit::builtin::detail::grouptransform_reg);
    engine.registerFunction("groupfilter",&::numkit::builtin::detail::groupfilter_reg);
    engine.registerFunction("colperm",    &::numkit::builtin::detail::colperm_reg);
    engine.registerFunction("symrcm",     &::numkit::builtin::detail::symrcm_reg);

    engine.registerFunction("full",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &) {
            if (args.empty())
                throw std::runtime_error("full requires 1 argument");
            outs[0] = args[0];
        });
}

} // namespace numkit::bundle::builtin
