import io

# 1. Header: overloads with the external-defs set.
p = r'C:\Users\User\Projects\megahard\numkit\src\core\include\nk\core\dependency_graph.hpp'
src = io.open(p, encoding='utf-8').read()
old = """    std::vector<size_t> dirtySetFor(const std::string &var) const;"""
new = """    std::vector<size_t> dirtySetFor(const std::string &var) const;

    /// With the set of EXTERNALLY-DEFINED params (ReactiveSession):
    /// the wildcard-prefix rule additionally skips the pure defs of
    /// EVERY param in the set (a prefix re-run must not clobber other
    /// sliders' values back to script defaults).
    std::vector<size_t> dirtySetFor(
        const std::string &var,
        const std::set<std::string> &externalDefs) const;"""
assert old in src
src = src.replace(old, new, 1)
old2 = """    std::vector<size_t> dirtySetForElement(const std::string &var,
                                           size_t r, size_t c) const;"""
new2 = """    std::vector<size_t> dirtySetForElement(const std::string &var,
                                           size_t r, size_t c) const;
    std::vector<size_t> dirtySetForElement(
        const std::string &var, size_t r, size_t c,
        const std::set<std::string> &externalDefs) const;"""
assert old2 in src
src = src.replace(old2, new2, 1)
io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
print("header overloads added")

# 2. Impl: rename bodies to take the set; thin wrappers delegate.
p = r'C:\Users\User\Projects\megahard\numkit\src\core\src\dependency_graph.cpp'
src = io.open(p, encoding='utf-8').read()

# dirtySetForElement: rename signature + param-skip.
old = """std::vector<size_t> DependencyGraph::dirtySetForElement(
    const std::string &var, size_t r, size_t c) const
{"""
new = """std::vector<size_t> DependencyGraph::dirtySetForElement(
    const std::string &var, size_t r, size_t c) const
{
    return dirtySetForElement(var, r, c, {});
}

std::vector<size_t> DependencyGraph::dirtySetForElement(
    const std::string &var, size_t r, size_t c,
    const std::set<std::string> &externalDefs) const
{"""
assert old in src
src = src.replace(old, new, 1)

# Its prefix lambda: extend the skip to externalDefs.
old = """    auto applyWildcardPrefix = [this, &dirty, &work, &var]() {
        bool again = true;
        while (again) {
            again = false;
            for (size_t i = 0; i < facts_.size(); ++i)
                if (dirty[i] && facts_[i].wildcardReads)
                    for (size_t k = 0; k < i; ++k) {
                        // The param's own pure definition stays
                        // skipped — re-running it would clobber the
                        // setParam write with the script default.
                        if (facts_[k].writes.count(var)
                            && !facts_[k].reads.count(var)
                            && !facts_[k].wildcardReads)
                            continue;
                        if (!dirty[k]) {
                            dirty[k] = 1;
                            work.push_back(k);
                            if (facts_[k].wildcardReads)
                                again = true;
                        }
                    }
        }
    };"""
new = """    auto applyWildcardPrefix =
        [this, &dirty, &work, &var, &externalDefs]() {
        bool again = true;
        while (again) {
            again = false;
            for (size_t i = 0; i < facts_.size(); ++i)
                if (dirty[i] && facts_[i].wildcardReads)
                    for (size_t k = 0; k < i; ++k) {
                        // Pure defs of the changed var AND of every
                        // externally-set param stay skipped —
                        // re-running them would clobber param values
                        // (the current slider OR another one) back to
                        // script defaults.
                        bool pureOfParam =
                            facts_[k].writes.count(var)
                            && !facts_[k].reads.count(var)
                            && !facts_[k].wildcardReads;
                        for (const auto &pv : externalDefs) {
                            if (facts_[k].writes.count(pv)
                                && !facts_[k].reads.count(pv)
                                && !facts_[k].wildcardReads) {
                                pureOfParam = true;
                                break;
                            }
                        }
                        if (pureOfParam)
                            continue;
                        if (!dirty[k]) {
                            dirty[k] = 1;
                            work.push_back(k);
                            if (facts_[k].wildcardReads)
                                again = true;
                        }
                    }
        }
    };"""
assert src.count(old) == 2
src = src.replace(old, new, 2)

# dirtySetFor: wrapper + impl signature.
old = """std::vector<size_t> DependencyGraph::dirtySetFor(const std::string &var) const
{
    std::vector<char> dirty(facts_.size(), 0);
    std::vector<size_t> work;"""
new = """std::vector<size_t> DependencyGraph::dirtySetFor(const std::string &var) const
{
    return dirtySetFor(var, {});
}

std::vector<size_t> DependencyGraph::dirtySetFor(
    const std::string &var,
    const std::set<std::string> &externalDefs) const
{
    std::vector<char> dirty(facts_.size(), 0);
    std::vector<size_t> work;"""
assert old in src
src = src.replace(old, new, 1)
io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
print("impl overloads wired")

# 3. Session: track params_, pass to graph.
p = r'C:\Users\User\Projects\megahard\numkit\src\core\include\nk\core\reactive_session.hpp'
src = io.open(p, encoding='utf-8').read()
old = """    std::set<size_t> dirty_;
    std::string lastError_;"""
new = """    std::set<size_t> dirty_;
    std::set<std::string> params_;   // externally-set variables
    std::string lastError_;"""
assert old in src
src = src.replace(old, new, 1)
io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
print("session header: params_")

p = r'C:\Users\User\Projects\megahard\numkit\src\core\src\reactive_session.cpp'
src = io.open(p, encoding='utf-8').read()
old = "    markDirty(graph_->dirtySetFor(var));"
new = """    params_.insert(var);
    markDirty(graph_->dirtySetFor(var, params_));"""
assert old in src
src = src.replace(old, new, 1)
old = "    markDirty(graph_->dirtySetForElement(var, r, c));"
new = """    params_.insert(var);
    markDirty(graph_->dirtySetForElement(var, r, c, params_));"""
assert old in src
src = src.replace(old, new, 1)
io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
print("session impl: params_ plumbed")
