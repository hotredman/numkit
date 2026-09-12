#include <numkit/core/play_manifest.hpp>

#include <cctype>

namespace numkit {

namespace {

bool isSpaceCh(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/// True when `pos` opens a manifest block: a `%{` whose following
/// whitespace leads to the `@play` marker (canonical) or `#ui` (legacy
/// alias). Returns the offset just past the marker, or npos.
size_t markerAfter(const std::string &src, size_t pos)
{
    size_t i = pos + 2;   // skip "%{"
    while (i < src.size() && isSpaceCh(src[i]))
        ++i;
    auto matches = [&](const char *marker) {
        size_t m = 0;
        while (marker[m])
            ++m;
        if (i + m > src.size())
            return false;
        for (size_t k = 0; k < m; ++k)
            if (src[i + k] != marker[k])
                return false;
        // The marker must be a standalone word — not `@playx`.
        char next = i + m < src.size() ? src[i + m] : '\0';
        return next == '\0' || isSpaceCh(next);
    };
    if (matches("@play"))
        return i + 5;
    if (matches("#ui"))
        return i + 3;
    return std::string::npos;
}

/// Pull `"var": "name"` and the matching `"default": <raw>` pairs out
/// of the manifest JSON. A light scan is deliberate: the engine only
/// needs the control variables; structural JSON parsing belongs to the
/// web layer. Objects are walked with brace depth so a nested
/// `"default"` inside an unrelated object is not mistaken for a
/// control's.
std::vector<PlayControl> scanControls(const std::string &json)
{
    std::vector<PlayControl> out;
    auto findKey = [&](size_t from, const char *key) {
        std::string pat = std::string("\"") + key + "\"";
        size_t p = from;
        while ((p = json.find(pat, p)) != std::string::npos) {
            size_t q = p + pat.size();
            while (q < json.size() && isSpaceCh(json[q]))
                ++q;
            if (q < json.size() && json[q] == ':')
                return q + 1;
            p = q;
        }
        return std::string::npos;
    };
    size_t pos = 0;
    while ((pos = findKey(pos, "var")) != std::string::npos) {
        size_t q = pos;
        while (q < json.size() && isSpaceCh(json[q]))
            ++q;
        if (q >= json.size() || json[q] != '"')
            continue;   // not a string value — skip
        size_t e = json.find('"', q + 1);
        if (e == std::string::npos)
            break;
        PlayControl c;
        c.var = json.substr(q + 1, e - q - 1);
        // The default lives in the same object: search forward for
        // "default" before the object's closing brace at this depth.
        size_t d = findKey(e, "default");
        if (d != std::string::npos) {
            // Take the raw value up to the next ',' or '}' at depth 0.
            while (d < json.size() && isSpaceCh(json[d]))
                ++d;
            size_t end = d;
            int depth = 0;
            bool inStr = false;
            while (end < json.size()) {
                char ch = json[end];
                if (inStr) {
                    if (ch == '\\')
                        ++end;
                    else if (ch == '"')
                        inStr = false;
                } else if (ch == '"') {
                    inStr = true;
                } else if (ch == '{' || ch == '[') {
                    ++depth;
                } else if (ch == '}' || ch == ']') {
                    if (depth == 0)
                        break;
                    --depth;
                } else if (ch == ',' && depth == 0) {
                    break;
                }
                ++end;
            }
            c.defaultRaw = json.substr(d, end - d);
            // Trim trailing whitespace.
            while (!c.defaultRaw.empty()
                   && isSpaceCh(c.defaultRaw.back()))
                c.defaultRaw.pop_back();
            pos = end;
        } else {
            pos = e;
        }
        if (!c.var.empty())
            out.push_back(std::move(c));
    }
    return out;
}

}  // namespace

PlayManifest extractPlayManifest(const std::string &source)
{
    PlayManifest m;
    size_t pos = 0;
    while ((pos = source.find("%{", pos)) != std::string::npos) {
        // Not a block-comment opener inside a string? The lexer's rule:
        // `%{` opens a block comment only at a line start (preceded by
        // whitespace). Mirror that to avoid grabbing `%{` inside text.
        if (pos > 0) {
            char prev = source[pos - 1];
            if (prev != '\n' && prev != '\r' && !isSpaceCh(prev)) {
                pos += 2;
                continue;
            }
        }
        size_t afterMarker = markerAfter(source, pos);
        if (afterMarker == std::string::npos) {
            pos += 2;
            continue;
        }
        size_t close = source.find("%}", afterMarker);
        if (close == std::string::npos)
            return m;   // unterminated block — treat as no manifest
        // Trim whitespace around the JSON payload.
        size_t b = afterMarker, e = close;
        while (b < e && isSpaceCh(source[b]))
            ++b;
        while (e > b && isSpaceCh(source[e - 1]))
            --e;
        if (b >= e)
            return m;   // marker without a payload
        m.rawJson = source.substr(b, e - b);
        m.controls = scanControls(m.rawJson);
        return m;       // first manifest wins
    }
    return m;
}

}  // namespace numkit
