/// @file figure_render.hpp
/// @ingroup group_graphics
///
/// Engine-side figure renderer — the software path behind
/// saveas/exportgraphics/print (portion 32). Produces a pixel-space
/// DISPLAY LIST from a FigureState, plus two backends:
///   • renderFigureSvg  — vector SVG (text as <text>)
///   • renderFigureRgb  — rasterised RGB buffer (built-in 5×7 bitmap
///     font, Bresenham lines, scanline polygon fill)
///
/// Deliberately STL-only and Value-free (same contract as the rest of
/// the figure layer); the bundle's export_reg.cpp turns the RGB buffer
/// into PNG bytes via the image toolbox and writes through the fs.
///
/// Scope (portion 32): vector chart types — line, stairs, stem, scatter,
/// bar, barh, area, errorbar — on linear/log axes with grid, limits,
/// tick formats, dir reverse, subplot tiling, legend, sgtitle.
/// NOT yet rendered: 3-D surfaces/contours, imagesc/heatmap (colormap
/// port), polar, text overlays, yyaxis right-scale, annotations —
/// tracked in bugs/opened/graphics/handle-surface-gaps.md sibling
/// export-gaps file.
#pragma once

#include <nk/figure/figure_manager.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace nk {
namespace figrender {

// ── Colour helpers ───────────────────────────────────────────────────
struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    Rgb() = default;
    Rgb(std::uint8_t rr, std::uint8_t gg, std::uint8_t bb)
        : r(rr), g(gg), b(bb) {}
    static Rgb fromHex(const std::string &hex)
    {
        if (hex.size() >= 7 && hex[0] == '#') {
            auto h = [&](size_t i) {
                return static_cast<std::uint8_t>(std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16));
            };
            return Rgb(h(1), h(3), h(5));
        }
        return Rgb(0, 0, 0);
    }
    std::string hex() const
    {
        char buf[8];
        std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
        return buf;
    }
};

// Series palette — the SAME hexes the IDE renderer's KIND_PALETTE uses,
// so saveas output matches the on-screen figure.
inline const std::vector<Rgb> &palette()
{
    static const std::vector<Rgb> p = {
        Rgb(0x7f, 0xd9, 0x9a), Rgb(0x5f, 0xb3, 0xd4), Rgb(0xe9, 0xb8, 0x70),
        Rgb(0x9b, 0x8c, 0xf2), Rgb(0xe2, 0x6a, 0x6a), Rgb(0xd4, 0xa5, 0xe6),
        Rgb(0xf2, 0xa3, 0x7e), Rgb(0x6f, 0xcf, 0xbf),
    };
    return p;
}

// MATLAB single-char linespec colours — mirrors the JS STYLE_COLOR map.
inline bool charSpecColor(char c, Rgb &out)
{
    switch (c) {
    case 'r': out = Rgb(0xf0, 0x70, 0x70); return true;
    case 'g': out = Rgb(0x6e, 0xe7, 0xa0); return true;
    case 'b': out = Rgb(0x60, 0xd0, 0xf0); return true;
    case 'k': out = Rgb(0x28, 0x28, 0x30); return true;
    case 'm': out = Rgb(0xe0, 0x70, 0xc0); return true;
    case 'c': out = Rgb(0x60, 0xd0, 0xf0); return true;
    case 'y': out = Rgb(0xe8, 0xd0, 0x60); return true;
    case 'w': out = Rgb(0xff, 0xff, 0xff); return true;
    default:  return false;
    }
}

// Parsed style string (both wire dialects: "r--o" and the kv extras).
struct StyleParts {
    Rgb color{0x7f, 0xd9, 0x9a};
    bool hasColor = false;   // explicit colour (char or kv)
    std::string lineStyle;   // "-", "--", ":", "-.", "" (unset), "none"
    std::string marker;      // "", "o", "+", "*", ".", "x", "s", "d", …
    // histogram extras (portion 39; engine histogram() packs them):
    Rgb faceColor{0x00, 0x72, 0xbd};   // MATLAB FaceColor 'auto'
    bool hasFaceColor = false;
    Rgb edgeColor{0xd9, 0xd9, 0xd9};   // MATLAB default light-gray edge
    bool hasEdgeColor = false;
    double binWidth = 0;      // 0 = derive from consecutive spacing
    double faceAlpha = 1;     // histogram FaceAlpha (default 0.6 comes
                              // in the kv from the engine)
};

// text() annotation extras, unpacked from the engine's ds.style
// "key=val;key=val" wire format (layout.cpp text() builds it):
//   color=<MATLAB colour spec>  rgb=r,g,b (0..1 floats)
//   fontSize=N                  halign=left|center|right
// Defaults are MATLAB R2025b probe-verified (2026-09-12): FontSize 10,
// HorizontalAlignment LEFT, VerticalAlignment middle, black.
struct TextExtras {
    Rgb color{0, 0, 0};
    double fontSize = 10;
    int halign = 0;          // 0 left, 1 center, 2 right
};

// One MATLAB colour spec → Rgb. Accepts the linespec short letters,
// long colour names, #rrggbb, and "r,g,b" / "r g b" float triplets
// (0..1, MATLAB convention). Returns false when unrecognised.
inline bool textColorSpec(const std::string &v, Rgb &out)
{
    // MATLAB-exact named colours (short letters are the SAME values:
    // 'r' == 'red' == (1,0,0)) — NOT the soft series palette, which is
    // a deliberate nk aesthetic for LINES only.
    static const std::pair<const char *, Rgb> kNames[] = {
        {"red", {255, 0, 0}},       {"green", {0, 128, 0}},
        {"blue", {0, 0, 255}},      {"cyan", {0, 255, 255}},
        {"magenta", {255, 0, 255}}, {"yellow", {255, 255, 0}},
        {"black", {0, 0, 0}},       {"white", {255, 255, 255}},
        {"none", {255, 255, 255}},
    };
    std::string s;
    for (char c : v)
        if (c != ' ' && c != '[' && c != ']')
            s += (char)std::tolower((unsigned char)c);
    if (s.size() == 1) {
        for (const auto &kv : kNames)
            if (s[0] == kv.first[0]) {
                out = kv.second;
                return true;
            }
        return false;
    }
    for (const auto &kv : kNames)
        if (s == kv.first) {
            out = kv.second;
            return true;
        }
    if (s.size() >= 7 && s[0] == '#') {
        out = Rgb::fromHex(s);
        return true;
    }
    double ch[3] = {0, 0, 0};
    int n = 0;
    size_t i = 0;
    while (i < s.size() && n < 3) {
        size_t e = i;
        while (e < s.size() && s[e] != ',')
            ++e;
        try {
            ch[n++] = std::stod(s.substr(i, e - i));
        } catch (...) {
            return false;
        }
        i = e + 1;
    }
    if (n == 3) {
        out = Rgb((std::uint8_t)std::lround(std::min(1.0, std::max(0.0, ch[0])) * 255),
                  (std::uint8_t)std::lround(std::min(1.0, std::max(0.0, ch[1])) * 255),
                  (std::uint8_t)std::lround(std::min(1.0, std::max(0.0, ch[2])) * 255));
        return true;
    }
    return false;
}

inline TextExtras parseTextExtras(const std::string &style)
{
    TextExtras t;
    if (style.empty())
        return t;
    size_t i = 0;
    while (i < style.size()) {
        size_t semi = style.find(';', i);
        std::string kv = style.substr(
            i, (semi == std::string::npos ? style.size() : semi) - i);
        size_t eq = kv.find('=');
        if (eq != std::string::npos) {
            std::string key = kv.substr(0, eq);
            std::string val = kv.substr(eq + 1);
            if (key == "color") {
                Rgb c;
                if (textColorSpec(val, c))
                    t.color = c;
            } else if (key == "rgb") {
                Rgb c;
                if (textColorSpec(val, c))
                    t.color = c;
            } else if (key == "fontSize") {
                try {
                    t.fontSize = std::stod(val);
                } catch (...) {
                }
            } else if (key == "halign") {
                t.halign = val == "center" ? 1 : val == "right" ? 2 : 0;
            }
        }
        i = semi == std::string::npos ? style.size() : semi + 1;
    }
    return t;
}

inline StyleParts parseStyle(const std::string &s)
{
    StyleParts p;
    if (s.empty())
        return p;
    if (s.find('=') != std::string::npos) {
        for (size_t i = 0; i < s.size();) {
            size_t eq = s.find('=', i);
            if (eq == std::string::npos)
                break;
            size_t semi = s.find(';', eq);
            std::string key = s.substr(i, eq - i);
            std::string val = s.substr(
                eq + 1, (semi == std::string::npos ? s.size() : semi) - eq - 1);
            if (key == "color") {
                p.color = Rgb::fromHex(val);
                p.hasColor = true;
            } else if (key == "lineStyle") {
                p.lineStyle = val;
            } else if (key == "marker") {
                p.marker = val;
            } else if (key == "faceColor") {
                p.faceColor = Rgb::fromHex(val);
                p.hasFaceColor = true;
            } else if (key == "edgeColor") {
                p.edgeColor = Rgb::fromHex(val);
                p.hasEdgeColor = true;
            } else if (key == "binWidth") {
                try {
                    p.binWidth = std::stod(val);
                } catch (...) {
                }
            } else if (key == "faceAlpha") {
                try {
                    p.faceAlpha = std::stod(val);
                } catch (...) {
                }
            }
            i = semi == std::string::npos ? s.size() : semi + 1;
        }
        return p;
    }
    static const std::string markers = "o+*.xsd^v<>ph";
    size_t i = 0;
    while (i < s.size()) {
        if (p.lineStyle.empty() && s.compare(i, 2, "--") == 0) { p.lineStyle = "--"; i += 2; continue; }
        if (p.lineStyle.empty() && s.compare(i, 2, "-.") == 0)  { p.lineStyle = "-.";  i += 2; continue; }
        char c = s[i];
        if (p.lineStyle.empty() && (c == '-' || c == ':')) { p.lineStyle = std::string(1, c); ++i; continue; }
        if (!p.hasColor) {
            Rgb col;
            if (charSpecColor(c, col)) {
                p.color = col;
                p.hasColor = true;
                ++i;
                continue;
            }
        }
        if (p.marker.empty() && markers.find(c) != std::string::npos) {
            p.marker = std::string(1, c);
            ++i;
            continue;
        }
        ++i;
    }
    return p;
}

// ── Colormap tables ──────────────────────────────────────────────────
// Ground truth: MATLAB R2025b feval(<name>, 256) dumped as hex RGB
// (2026-09-11 probe; first parula entry 3e26a8 = (62,38,168) verified
// against exported-PNG pixels). MATLAB maps a scaled CData value v to
// entry round(t*(N-1)) + 1 (1-based) with t = (v-clo)/(chi-clo)
// clamped to [0,1]; NaN renders as entry 1 (the first color).
// viridis: MATLAB R2025b has no viridis() (exist()==0) — this table is
// the matplotlib-approximate stops from the IDE colormaps.js, linearly
// sampled to 256, so engine export and IDE render identically.
struct NamedPaletteHex { const char *name; const char *hex; };
inline const NamedPaletteHex kNamedPaletteHex[] = {
    {"parula", "3e26a83e27ac3f28af3f29b2402ab4402bb7412cba412dbd422ebf422fc24330c54331c84332ca4433cd4434d04535d24537d54538d74639d9463adc463bde463de0473ee1473fe34741e54742e64744e84745e94746eb4848ec4849ed484bee484cf0484ef1484ff24850f34852f44853f54854f64756f74757f74759f8475af9475bfa475dfa465efb4660fb4661fc4562fc4564fd4465fd4367fd4368fe426afe416bfe406dfe3f6eff3e70ff3c71ff3b73ff3974ff3876fe3677fe3579fd337afd327cfc317dfc307ffb2f80fa2f82fa2e83f92e84f82e86f82e87f72d88f62d8af52d8bf42d8cf32d8ef22c8ff12c90f02b91ef2a93ee2994ed2895ec2797eb2798ea2699e9269ae8259be8259ce7249ee6249fe523a0e523a1e422a2e421a3e320a5e31fa6e21ea7e11da8e11da9e01caadf1babde1aacdd19addc17aeda16afd914b0d812b1d610b2d50eb3d40bb3d208b4d106b5cf04b6ce02b7cc01b7ca00b8c900b9c700bac601bac402bbc204bbc106bcbf09bdbd0dbdbc10beba14beb817bfb61ac0b51dc0b320c1b123c1af25c2ae27c2ac29c3aa2bc3a82cc4a62ec4a52fc5a331c5a132c69f33c79d35c79b36c89938c89639c9943bc9923dca9040ca8d42ca8b45cb8948cb864bcb844ecc8151cc7f54cc7c57cc7a5acc775ecd7461cd7264cd6f67cd6c6bcd696ecd6672cd6476cc6179cc5e7dcc5b81cc5984cc5688cb538bcb518fcb4e93ca4b96ca489ac9469dc943a1c840a4c83ea7c73babc739aec637b2c635b5c533b8c431bbc42fbec32dc2c32cc5c22ac8c129cbc128cec027d0bf27d3bf27d6be27d9be28dbbd28debc29e1bc2ae3bc2be6bb2de8bb2eeaba30ecba32efba35f1ba37f3ba39f5ba3bf7ba3df9ba3efbbb3efcbc3efebd3dfebe3cfec03bfec13afec239fec438fec537fec735fec834feca33fdcb32fdcd31fdce31fcd030fbd22ffbd32efad52ef9d62df9d82cf8d92bf7db2af7dd2af6de29f6e028f5e128f5e327f5e526f5e626f5e825f5e924f5eb23f5ec22f5ee21f6ef20f6f11ff6f21ef7f41cf7f51bf8f71af8f818f9f916f9fb15"},
    {"turbo", "30123b32154333184a341b51351e5836215f37246638276d392a733a2d793b2f803c32863d358b3e38913f3b973f3e9c4040a24143a74146ac4249b1424bb5434eba4451bf4454c34456c74559cb455ccf455ed34661d64664da4666dd4669e0466be3476ee64771e94773eb4776ee4778f0477bf2467df44680f64682f84685fa4687fb458afc458cfd448ffe4391fe4294ff4196ff4099ff3e9bfe3d9efe3ba0fd3aa3fc38a5fb37a8fa35abf833adf731aff52fb2f42eb4f22cb7f02ab9ee28bceb27bee925c0e723c3e422c5e220c7df1fc9dd1ecbda1ccdd81bd0d51ad2d21ad4d019d5cd18d7ca18d9c818dbc518ddc218dec018e0bd19e2bb19e3b91ae4b61ce6b41de7b21fe9af20eaac22ebaa25eca727eea42aefa12cf09e2ff19b32f29835f39438f4913cf58e3ff68a43f78746f8844af8804ef97d52fa7a55fa7659fb735dfc6f61fc6c65fd6969fd666dfe6271fe5f75fe5c79fe597dff5680ff5384ff5188ff4e8bff4b8fff4992ff4796fe4499fe429cfe409ffd3fa1fd3da4fc3ca7fc3aa9fb39acfb38affa37b1f936b4f836b7f735b9f635bcf534bef434c1f334c3f134c6f034c8ef34cbed34cdec34d0ea34d2e935d4e735d7e535d9e436dbe236dde037dfdf37e1dd37e3db38e5d938e7d739e9d539ebd339ecd13aeecf3aefcd3af1cb3af2c93af4c73af5c53af6c33af7c13af8be39f9bc39faba39fbb838fbb637fcb336fcb136fdae35fdac34fea933fea732fea431fea130fe9e2ffe9b2dfe992cfe962bfe932afe9029fd8d27fd8a26fc8725fc8423fb8122fb7e21fa7b1ff9781ef9751df8721cf76f1af66c19f56918f46617f36315f26014f15d13f05b12ef5811ed5510ec530feb500eea4e0de84b0ce7490ce5470be4450ae2430ae14109df3f08dd3d08dc3b07da3907d83706d63506d43305d23105d02f05ce2d04cc2b04ca2a04c82803c52603c32503c12302be2102bc2002b91e02b71d02b41b01b21a01af1801ac1701a91601a71401a41301a112019e10019b0f01980e01950d01920b018e0a018b09028808028507028106027e05027a0403"},
    {"jet", "00008300008700008b00008f00009300009700009b00009f0000a30000a70000ab0000af0000b30000b70000bb0000bf0000c30000c70000cb0000cf0000d30000d70000db0000df0000e30000e70000eb0000ef0000f30000f70000fb0000ff0004ff0008ff000cff0010ff0014ff0018ff001cff0020ff0024ff0028ff002cff0030ff0034ff0038ff003cff0040ff0044ff0048ff004cff0050ff0054ff0058ff005cff0060ff0064ff0068ff006cff0070ff0074ff0078ff007cff0080ff0083ff0087ff008bff008fff0093ff0097ff009bff009fff00a3ff00a7ff00abff00afff00b3ff00b7ff00bbff00bfff00c3ff00c7ff00cbff00cfff00d3ff00d7ff00dbff00dfff00e3ff00e7ff00ebff00efff00f3ff00f7ff00fbff00ffff04fffb08fff70cfff310ffef14ffeb18ffe71cffe320ffdf24ffdb28ffd72cffd330ffcf34ffcb38ffc73cffc340ffbf44ffbb48ffb74cffb350ffaf54ffab58ffa75cffa360ff9f64ff9b68ff976cff9370ff8f74ff8b78ff877cff8380ff8083ff7c87ff788bff748fff7093ff6c97ff689bff649fff60a3ff5ca7ff58abff54afff50b3ff4cb7ff48bbff44bfff40c3ff3cc7ff38cbff34cfff30d3ff2cd7ff28dbff24dfff20e3ff1ce7ff18ebff14efff10f3ff0cf7ff08fbff04ffff00fffb00fff700fff300ffef00ffeb00ffe700ffe300ffdf00ffdb00ffd700ffd300ffcf00ffcb00ffc700ffc300ffbf00ffbb00ffb700ffb300ffaf00ffab00ffa700ffa300ff9f00ff9b00ff9700ff9300ff8f00ff8b00ff8700ff8300ff8000ff7c00ff7800ff7400ff7000ff6c00ff6800ff6400ff6000ff5c00ff5800ff5400ff5000ff4c00ff4800ff4400ff4000ff3c00ff3800ff3400ff3000ff2c00ff2800ff2400ff2000ff1c00ff1800ff1400ff1000ff0c00ff0800ff0400ff0000fb0000f70000f30000ef0000eb0000e70000e30000df0000db0000d70000d30000cf0000cb0000c70000c30000bf0000bb0000b70000b30000af0000ab0000a70000a300009f00009b00009700009300008f00008b0000870000830000800000"},
    {"hot", "0300000500000800000b00000d00001000001300001500001800001b00001d00002000002300002500002800002b00002d00003000003200003500003800003a00003d00004000004200004500004800004a00004d00005000005200005500005800005a00005d00006000006200006500006800006a00006d00007000007200007500007800007a00007d00008000008200008500008700008a00008d00008f00009200009500009700009a00009d00009f0000a20000a50000a70000aa0000ad0000af0000b20000b50000b70000ba0000bd0000bf0000c20000c50000c70000ca0000cd0000cf0000d20000d50000d70000da0000dc0000df0000e20000e40000e70000ea0000ec0000ef0000f20000f40000f70000fa0000fc0000ff0000ff0300ff0500ff0800ff0b00ff0d00ff1000ff1300ff1500ff1800ff1b00ff1d00ff2000ff2300ff2500ff2800ff2b00ff2d00ff3000ff3200ff3500ff3800ff3a00ff3d00ff4000ff4200ff4500ff4800ff4a00ff4d00ff5000ff5200ff5500ff5800ff5a00ff5d00ff6000ff6200ff6500ff6800ff6a00ff6d00ff7000ff7200ff7500ff7800ff7a00ff7d00ff8000ff8200ff8500ff8700ff8a00ff8d00ff8f00ff9200ff9500ff9700ff9a00ff9d00ff9f00ffa200ffa500ffa700ffaa00ffad00ffaf00ffb200ffb500ffb700ffba00ffbd00ffbf00ffc200ffc500ffc700ffca00ffcd00ffcf00ffd200ffd500ffd700ffda00ffdc00ffdf00ffe200ffe400ffe700ffea00ffec00ffef00fff200fff400fff700fffa00fffc00ffff00ffff04ffff08ffff0cffff10ffff14ffff18ffff1cffff20ffff24ffff28ffff2cffff30ffff34ffff38ffff3cffff40ffff44ffff48ffff4cffff50ffff54ffff58ffff5cffff60ffff64ffff68ffff6cffff70ffff74ffff78ffff7cffff80ffff83ffff87ffff8bffff8fffff93ffff97ffff9bffff9fffffa3ffffa7ffffabffffafffffb3ffffb7ffffbbffffbfffffc3ffffc7ffffcbffffcfffffd3ffffd7ffffdbffffdfffffe3ffffe7ffffebffffeffffff3fffff7fffffbffffff"},
    {"cool", "00ffff01feff02fdff03fcff04fbff05faff06f9ff07f8ff08f7ff09f6ff0af5ff0bf4ff0cf3ff0df2ff0ef1ff0ff0ff10efff11eeff12edff13ecff14ebff15eaff16e9ff17e8ff18e7ff19e6ff1ae5ff1be4ff1ce3ff1de2ff1ee1ff1fe0ff20dfff21deff22ddff23dcff24dbff25daff26d9ff27d8ff28d7ff29d6ff2ad5ff2bd4ff2cd3ff2dd2ff2ed1ff2fd0ff30cfff31ceff32cdff33ccff34cbff35caff36c9ff37c8ff38c7ff39c6ff3ac5ff3bc4ff3cc3ff3dc2ff3ec1ff3fc0ff40bfff41beff42bdff43bcff44bbff45baff46b9ff47b8ff48b7ff49b6ff4ab5ff4bb4ff4cb3ff4db2ff4eb1ff4fb0ff50afff51aeff52adff53acff54abff55aaff56a9ff57a8ff58a7ff59a6ff5aa5ff5ba4ff5ca3ff5da2ff5ea1ff5fa0ff609fff619eff629dff639cff649bff659aff6699ff6798ff6897ff6996ff6a95ff6b94ff6c93ff6d92ff6e91ff6f90ff708fff718eff728dff738cff748bff758aff7689ff7788ff7887ff7986ff7a85ff7b84ff7c83ff7d82ff7e81ff7f80ff807fff817eff827dff837cff847bff857aff8679ff8778ff8877ff8976ff8a75ff8b74ff8c73ff8d72ff8e71ff8f70ff906fff916eff926dff936cff946bff956aff9669ff9768ff9867ff9966ff9a65ff9b64ff9c63ff9d62ff9e61ff9f60ffa05fffa15effa25dffa35cffa45bffa55affa659ffa758ffa857ffa956ffaa55ffab54ffac53ffad52ffae51ffaf50ffb04fffb14effb24dffb34cffb44bffb54affb649ffb748ffb847ffb946ffba45ffbb44ffbc43ffbd42ffbe41ffbf40ffc03fffc13effc23dffc33cffc43bffc53affc639ffc738ffc837ffc936ffca35ffcb34ffcc33ffcd32ffce31ffcf30ffd02fffd12effd22dffd32cffd42bffd52affd629ffd728ffd827ffd926ffda25ffdb24ffdc23ffdd22ffde21ffdf20ffe01fffe11effe21dffe31cffe41bffe51affe619ffe718ffe817ffe916ffea15ffeb14ffec13ffed12ffee11ffef10fff00ffff10efff20dfff30cfff40bfff50afff609fff708fff807fff906fffa05fffb04fffc03fffd02fffe01ffff00ff"},
    {"gray", "0000000101010202020303030404040505050606060707070808080909090a0a0a0b0b0b0c0c0c0d0d0d0e0e0e0f0f0f1010101111111212121313131414141515151616161717171818181919191a1a1a1b1b1b1c1c1c1d1d1d1e1e1e1f1f1f2020202121212222222323232424242525252626262727272828282929292a2a2a2b2b2b2c2c2c2d2d2d2e2e2e2f2f2f3030303131313232323333333434343535353636363737373838383939393a3a3a3b3b3b3c3c3c3d3d3d3e3e3e3f3f3f4040404141414242424343434444444545454646464747474848484949494a4a4a4b4b4b4c4c4c4d4d4d4e4e4e4f4f4f5050505151515252525353535454545555555656565757575858585959595a5a5a5b5b5b5c5c5c5d5d5d5e5e5e5f5f5f6060606161616262626363636464646565656666666767676868686969696a6a6a6b6b6b6c6c6c6d6d6d6e6e6e6f6f6f7070707171717272727373737474747575757676767777777878787979797a7a7a7b7b7b7c7c7c7d7d7d7e7e7e7f7f7f8080808181818282828383838484848585858686868787878888888989898a8a8a8b8b8b8c8c8c8d8d8d8e8e8e8f8f8f9090909191919292929393939494949595959696969797979898989999999a9a9a9b9b9b9c9c9c9d9d9d9e9e9e9f9f9fa0a0a0a1a1a1a2a2a2a3a3a3a4a4a4a5a5a5a6a6a6a7a7a7a8a8a8a9a9a9aaaaaaabababacacacadadadaeaeaeafafafb0b0b0b1b1b1b2b2b2b3b3b3b4b4b4b5b5b5b6b6b6b7b7b7b8b8b8b9b9b9babababbbbbbbcbcbcbdbdbdbebebebfbfbfc0c0c0c1c1c1c2c2c2c3c3c3c4c4c4c5c5c5c6c6c6c7c7c7c8c8c8c9c9c9cacacacbcbcbcccccccdcdcdcecececfcfcfd0d0d0d1d1d1d2d2d2d3d3d3d4d4d4d5d5d5d6d6d6d7d7d7d8d8d8d9d9d9dadadadbdbdbdcdcdcdddddddedededfdfdfe0e0e0e1e1e1e2e2e2e3e3e3e4e4e4e5e5e5e6e6e6e7e7e7e8e8e8e9e9e9eaeaeaebebebecececedededeeeeeeefefeff0f0f0f1f1f1f2f2f2f3f3f3f4f4f4f5f5f5f6f6f6f7f7f7f8f8f8f9f9f9fafafafbfbfbfcfcfcfdfdfdfefefeffffff"},
    {"bone", "00000001010202020303030404040504040605050806060907070a08080b09090c0a0a0e0b0b0f0b0b100c0c110d0d120e0e140f0f1510101611111712121812121a13131b14141c15151d16161f1717201818211818221919231a1a251b1b261c1c271d1d281e1e291f1f2b20202c20202d21212e22222f2323312424322525332626342727352727372828382929392a2a3a2b2b3b2c2c3d2d2d3e2e2e3f2e2e402f2f4230304331314432324533334634344835354935354a36364b37374c38384e39394f3a3a503b3b513c3c523c3c543d3d553e3e563f3f5740405841415a42425b43435c43435d44445e4545604646614747624848634949654a4a664a4a674b4b684c4c694d4d6b4e4e6c4f4f6d50506e51516f515171525272535373545474555675565776575877585977585a78595c795a5d7a5b5e7b5c5f7c5d607d5e627e5f637e5f647f606580616681626882636983646a84656b84666c85666e86676f876870886971896a738a6b748b6c758c6d768c6d778d6e798e6f7a8f707b90717c91727d92737f937480937481947582957683967785977886987987997a889a7b899a7b8b9b7c8c9c7d8d9d7e8e9e7f8f9f8091a08192a18293a18294a28396a38497a48598a58699a6879aa7889ca8899da8899ea98a9faa8ba0ab8ca2ac8da3ad8ea4ae8fa5af90a6af90a8b091a9b192aab293abb394acb495aeb596afb597b0b697b1b798b2b899b4b99ab5ba9bb6bb9cb7bc9db9bc9ebabd9ebbbe9fbcbfa0bdc0a1bfc1a2c0c2a3c1c3a4c2c3a5c3c4a5c5c5a6c6c6a7c7c7a8c8c8aac9c9abcacaadcbcbaecbcbafccccb1cdcdb2ceceb3cfcfb5d0d0b6d1d1b8d2d2b9d2d2bad3d3bcd4d4bdd5d5bed6d6c0d7d7c1d8d8c3d9d9c4d9d9c5dadac7dbdbc8dcdcc9ddddcbdedeccdfdfcee0e0cfe0e0d0e1e1d2e2e2d3e3e3d4e4e4d6e5e5d7e6e6d9e6e6dae7e7dbe8e8dde9e9deeaeadfebebe1ecece2edede4edede5eeeee6efefe8f0f0e9f1f1eaf2f2ecf3f3edf4f4eff4f4f0f5f5f1f6f6f3f7f7f4f8f8f5f9f9f7fafaf8fbfbfafbfbfbfcfcfcfdfdfefefeffffff"},
    {"copper", "0000000101000302010402010503020604020805030905030a06040b07040d08050e09050f0906100a06120b07130c07140c08150d08160e09180f0919100a1a100a1c110b1d120b1e130c1f140c21140d22150d23160e24170e26170f27180f281910291a102b1b112c1b112d1c122e1d12301e13311e13321f143320143521153622153722163823163a24173b25173c25183d26183f271940281941291a42291a442a1b452b1b462c1c472d1c492d1d4a2e1d4b2f1e4c301e4e301f4f311f5032205133205334215434215535225636225837235937235a38245b39245d3a255e3b255f3b26603c26623d27633e27643e28653f2867402968412969422a6a422a6c432b6d442b6e452c6f462c71462d72472d73482e74492e76492f774a2f784b30794c307b4d317c4d317d4e327e4f328050338150338251348352348553358654358754368855368a56378b57378c57388d58388f5939905a39915b3a925b3a945c3b955d3b965e3c975f3c995f3d9a603d9b613e9c623e9e623f9f633fa06440a16540a36641a46641a56742a66842a86943a96943aa6a44ab6b44ac6c45ae6d45af6d46b06e46b26f47b37047b47048b57148b67249b87349b9744aba744abc754bbd764bbe774cbf784cc1784dc2794dc37a4ec47b4ec67b4fc77c4fc87d50c97e50cb7f51cc7f51cd8052ce8152d08253d18253d28354d38454d48555d68655d78656d88756da8857db8957dc8958dd8a58df8b59e08c59e18d5ae28d5ae48e5be58f5be6905ce7915ce9915dea925deb935eec945eee945fef955ff09660f19760f39861f49861f59962f69a62f89b63f99b63fa9c64fb9d64fc9e64fe9f65ff9f65ffa066ffa166ffa267ffa267ffa368ffa468ffa569ffa669ffa66affa76affa86bffa96bffaa6cffaa6cffab6dffac6dffad6effad6effae6fffaf6fffb070ffb170ffb171ffb271ffb372ffb472ffb473ffb573ffb674ffb774ffb875ffb875ffb976ffba76ffbb77ffbb77ffbc78ffbd78ffbe79ffbf79ffbf7affc07affc17bffc27bffc37cffc37cffc47dffc57dffc67effc67effc77f"},
    {"spring", "ff00ffff01feff02fdff03fcff04fbff05faff06f9ff07f8ff08f7ff09f6ff0af5ff0bf4ff0cf3ff0df2ff0ef1ff0ff0ff10efff11eeff12edff13ecff14ebff15eaff16e9ff17e8ff18e7ff19e6ff1ae5ff1be4ff1ce3ff1de2ff1ee1ff1fe0ff20dfff21deff22ddff23dcff24dbff25daff26d9ff27d8ff28d7ff29d6ff2ad5ff2bd4ff2cd3ff2dd2ff2ed1ff2fd0ff30cfff31ceff32cdff33ccff34cbff35caff36c9ff37c8ff38c7ff39c6ff3ac5ff3bc4ff3cc3ff3dc2ff3ec1ff3fc0ff40bfff41beff42bdff43bcff44bbff45baff46b9ff47b8ff48b7ff49b6ff4ab5ff4bb4ff4cb3ff4db2ff4eb1ff4fb0ff50afff51aeff52adff53acff54abff55aaff56a9ff57a8ff58a7ff59a6ff5aa5ff5ba4ff5ca3ff5da2ff5ea1ff5fa0ff609fff619eff629dff639cff649bff659aff6699ff6798ff6897ff6996ff6a95ff6b94ff6c93ff6d92ff6e91ff6f90ff708fff718eff728dff738cff748bff758aff7689ff7788ff7887ff7986ff7a85ff7b84ff7c83ff7d82ff7e81ff7f80ff807fff817eff827dff837cff847bff857aff8679ff8778ff8877ff8976ff8a75ff8b74ff8c73ff8d72ff8e71ff8f70ff906fff916eff926dff936cff946bff956aff9669ff9768ff9867ff9966ff9a65ff9b64ff9c63ff9d62ff9e61ff9f60ffa05fffa15effa25dffa35cffa45bffa55affa659ffa758ffa857ffa956ffaa55ffab54ffac53ffad52ffae51ffaf50ffb04fffb14effb24dffb34cffb44bffb54affb649ffb748ffb847ffb946ffba45ffbb44ffbc43ffbd42ffbe41ffbf40ffc03fffc13effc23dffc33cffc43bffc53affc639ffc738ffc837ffc936ffca35ffcb34ffcc33ffcd32ffce31ffcf30ffd02fffd12effd22dffd32cffd42bffd52affd629ffd728ffd827ffd926ffda25ffdb24ffdc23ffdd22ffde21ffdf20ffe01fffe11effe21dffe31cffe41bffe51affe619ffe718ffe817ffe916ffea15ffeb14ffec13ffed12ffee11ffef10fff00ffff10efff20dfff30cfff40bfff50afff609fff708fff807fff906fffa05fffb04fffc03fffd02fffe01ffff00"},
    {"summer", "0080660180660281660381660482660582660683660783660884660984660a85660b85660c86660d86660e87660f8766108866118866128966138966148a66158a66168b66178b66188c66198c661a8d661b8d661c8e661d8e661e8f661f8f662090662190662291662391662492662592662693662793662893662994662a95662b95662c96662d96662e97662f9766309866319866329966339966349a66359a66369b66379b66389c66399c663a9d663b9d663c9e663d9e663e9f663f9f6640a06641a06642a16643a16644a26645a26646a36647a36648a36649a4664aa5664ba5664ca6664da6664ea7664fa76650a86651a86652a96653a96654aa6655aa6656ab6657ab6658ac6659ac665aad665bad665cae665dae665eaf665faf6660b06661b06662b16663b16664b26665b26666b36667b36668b36669b4666ab5666bb5666cb6666db6666eb7666fb76670b86671b86672b96673b96674ba6675ba6676bb6677bb6678bc6679bc667abd667bbd667cbe667dbe667ebf667fbf6680c06681c06682c16683c16684c26685c26686c36687c36688c36689c4668ac4668bc5668cc6668dc6668ec7668fc76690c86691c86692c96693c96694ca6695ca6696cb6697cb6698cc6699cc669acd669bcd669cce669dce669ecf669fcf66a0d066a1d066a2d166a3d166a4d266a5d266a6d366a7d366a8d366a9d466aad466abd566acd666add666aed766afd766b0d866b1d866b2d966b3d966b4da66b5da66b6db66b7db66b8dc66b9dc66badd66bbdd66bcde66bdde66bedf66bfdf66c0e066c1e066c2e166c3e166c4e266c5e266c6e366c7e366c8e366c9e466cae466cbe566cce666cde666cee766cfe766d0e866d1e866d2e966d3e966d4ea66d5ea66d6eb66d7eb66d8ec66d9ec66daed66dbed66dcee66ddee66deef66dfef66e0f066e1f066e2f166e3f166e4f266e5f266e6f366e7f366e8f366e9f466eaf466ebf566ecf666edf666eef766eff766f0f866f1f866f2f966f3f966f4fa66f5fa66f6fb66f7fb66f8fc66f9fc66fafd66fbfd66fcfe66fdfe66feff66ffff66"},
    {"autumn", "ff0000ff0100ff0200ff0300ff0400ff0500ff0600ff0700ff0800ff0900ff0a00ff0b00ff0c00ff0d00ff0e00ff0f00ff1000ff1100ff1200ff1300ff1400ff1500ff1600ff1700ff1800ff1900ff1a00ff1b00ff1c00ff1d00ff1e00ff1f00ff2000ff2100ff2200ff2300ff2400ff2500ff2600ff2700ff2800ff2900ff2a00ff2b00ff2c00ff2d00ff2e00ff2f00ff3000ff3100ff3200ff3300ff3400ff3500ff3600ff3700ff3800ff3900ff3a00ff3b00ff3c00ff3d00ff3e00ff3f00ff4000ff4100ff4200ff4300ff4400ff4500ff4600ff4700ff4800ff4900ff4a00ff4b00ff4c00ff4d00ff4e00ff4f00ff5000ff5100ff5200ff5300ff5400ff5500ff5600ff5700ff5800ff5900ff5a00ff5b00ff5c00ff5d00ff5e00ff5f00ff6000ff6100ff6200ff6300ff6400ff6500ff6600ff6700ff6800ff6900ff6a00ff6b00ff6c00ff6d00ff6e00ff6f00ff7000ff7100ff7200ff7300ff7400ff7500ff7600ff7700ff7800ff7900ff7a00ff7b00ff7c00ff7d00ff7e00ff7f00ff8000ff8100ff8200ff8300ff8400ff8500ff8600ff8700ff8800ff8900ff8a00ff8b00ff8c00ff8d00ff8e00ff8f00ff9000ff9100ff9200ff9300ff9400ff9500ff9600ff9700ff9800ff9900ff9a00ff9b00ff9c00ff9d00ff9e00ff9f00ffa000ffa100ffa200ffa300ffa400ffa500ffa600ffa700ffa800ffa900ffaa00ffab00ffac00ffad00ffae00ffaf00ffb000ffb100ffb200ffb300ffb400ffb500ffb600ffb700ffb800ffb900ffba00ffbb00ffbc00ffbd00ffbe00ffbf00ffc000ffc100ffc200ffc300ffc400ffc500ffc600ffc700ffc800ffc900ffca00ffcb00ffcc00ffcd00ffce00ffcf00ffd000ffd100ffd200ffd300ffd400ffd500ffd600ffd700ffd800ffd900ffda00ffdb00ffdc00ffdd00ffde00ffdf00ffe000ffe100ffe200ffe300ffe400ffe500ffe600ffe700ffe800ffe900ffea00ffeb00ffec00ffed00ffee00ffef00fff000fff100fff200fff300fff400fff500fff600fff700fff800fff900fffa00fffb00fffc00fffd00fffe00ffff00"},
    {"winter", "0000ff0001ff0002fe0003fe0004fd0005fd0006fc0007fc0008fb0009fb000afa000bfa000cf9000df9000ef8000ff80010f70011f70012f60013f60014f50015f40016f40017f30018f30019f3001af2001bf2001cf1001df1001ef0001ff00020ef0021ef0022ee0023ee0024ed0025ed0026ec0027ec0028eb0029eb002aea002bea002ce9002de9002ee8002fe80030e70031e70032e60033e60034e50035e40036e40037e30038e30039e3003ae2003be2003ce1003de1003ee0003fe00040df0041df0042de0043de0044dd0045dd0046dc0047dc0048db0049db004ada004bda004cd9004dd9004ed8004fd80050d70051d70052d60053d60054d50055d50056d40057d30058d30059d3005ad2005bd2005cd1005dd1005ed0005fd00060cf0061cf0062ce0063ce0064cd0065cd0066cc0067cc0068cb0069cb006aca006bca006cc9006dc9006ec8006fc80070c70071c70072c60073c60074c50075c50076c40077c30078c30079c3007ac2007bc2007cc1007dc1007ec0007fc00080bf0081bf0082be0083be0084bd0085bd0086bc0087bc0088bb0089bb008aba008bba008cb9008db9008eb8008fb80090b70091b70092b60093b60094b50095b40096b40097b30098b30099b3009ab2009bb2009cb1009db1009eb0009fb000a0af00a1af00a2ae00a3ae00a4ad00a5ad00a6ac00a7ac00a8ab00a9ab00aaaa00abaa00aca900ada900aea800afa800b0a700b1a700b2a600b3a600b4a500b5a400b6a400b7a300b8a300b9a300baa200bba200bca100bda100bea000bfa000c09f00c19f00c29e00c39e00c49d00c59d00c69c00c79c00c89b00c99b00ca9a00cb9a00cc9900cd9900ce9800cf9800d09700d19700d29600d39600d49500d59400d69400d79300d89300d99300da9200db9200dc9100dd9100de9000df9000e08f00e18f00e28e00e38e00e48d00e58d00e68c00e78c00e88b00e98b00ea8a00eb8a00ec8900ed8900ee8800ef8800f08700f18700f28600f38600f48500f58400f68400f78400f88300f98300fa8200fb8200fc8100fd8100fe8000ff80"},
    {"hsv", "ff0000ff0600ff0c00ff1200ff1800ff1e00ff2400ff2a00ff3000ff3600ff3c00ff4200ff4800ff4e00ff5400ff5a00ff6000ff6600ff6c00ff7200ff7800ff7e00ff8300ff8900ff8f00ff9500ff9b00ffa100ffa700ffad00ffb300ffb900ffbf00ffc500ffcb00ffd100ffd700ffdd00ffe300ffe900ffef00fff500fffb00fdff00f7ff00f1ff00ebff00e5ff00dfff00d9ff00d3ff00cdff00c7ff00c1ff00bbff00b5ff00afff00a9ff00a3ff009dff0097ff0091ff008bff0085ff0080ff007aff0074ff006eff0068ff0062ff005cff0056ff0050ff004aff0044ff003eff0038ff0032ff002cff0026ff0020ff001aff0014ff000eff0008ff0002ff0000ff0400ff0a00ff1000ff1600ff1c00ff2200ff2800ff2e00ff3400ff3a00ff4000ff4600ff4c00ff5200ff5800ff5e00ff6400ff6a00ff7000ff7600ff7c00ff8100ff8700ff8d00ff9300ff9900ff9f00ffa500ffab00ffb100ffb700ffbd00ffc300ffc900ffcf00ffd500ffdb00ffe100ffe700ffed00fff300fff900ffff00f9ff00f3ff00edff00e7ff00e1ff00dbff00d5ff00cfff00c9ff00c3ff00bdff00b7ff00b1ff00abff00a5ff009fff0099ff0093ff008dff0087ff0081ff007cff0076ff0070ff006aff0064ff005eff0058ff0052ff004cff0046ff0040ff003aff0034ff002eff0028ff0022ff001cff0016ff0010ff000aff0004ff0200ff0800ff0e00ff1400ff1a00ff2000ff2600ff2c00ff3200ff3800ff3e00ff4400ff4a00ff5000ff5600ff5c00ff6200ff6800ff6e00ff7400ff7a00ff8000ff8500ff8b00ff9100ff9700ff9d00ffa300ffa900ffaf00ffb500ffbb00ffc100ffc700ffcd00ffd300ffd900ffdf00ffe500ffeb00fff100fff700fffd00ffff00fbff00f5ff00efff00e9ff00e3ff00ddff00d7ff00d1ff00cbff00c5ff00bfff00b9ff00b3ff00adff00a7ff00a1ff009bff0095ff008fff0089ff0083ff007eff0078ff0072ff006cff0066ff0060ff005aff0054ff004eff0048ff0042ff003cff0036ff0030ff002aff0024ff001eff0018ff0012ff000cff0006"},
    {"viridis", "44015444035544045745055845075945085a45095c450b5d450c5e450d5f460f6146106246126346136546146646166747176847186a471a6b471b6c471c6d471e6f471f70482071482273482374482475482676472776472877472978462b79462c79462d7a462e7b45307b45317c45327d44337e44357e44367f443780433881433a81433b82423c83423d83423f844140854141864142864144874045874046883f47883f48883e49883e4a893d4b893d4c893c4d893c4e893b4f8a3b508a3b518a3a538a3a548b39558b39568b38578b38588c37598c375a8c365b8c365c8c355d8d355e8d355f8d34608d34618d33628d33638d32648d32658d32668d31678d31688e30698e306a8e2f6b8e2f6c8e2e6d8e2e6e8e2e6f8e2d708e2d718e2c728e2c738e2b748e2b758e2b768e2a778e2a788e29798e297a8e297b8e287c8e287d8e287e8e277f8e27808e27818e26828e26838e25848d25848d25858d24868d24878d24888d23898d238a8d238b8d228c8d228d8d228e8d218f8d21908d21918c21928c21938c21938b21948b21958b21968a21978a21988a219989219a89219b89229c88229d88229e88229f8722a08722a08722a18622a28622a38622a48522a58522a68522a78422a88424a98325aa8226ab8228ac8129ac802aad7f2cae7f2daf7e2eb07d30b17c31b27c32b37b34b47a35b57936b57937b67839b7773ab8763bb9753dba753ebb743fbc7341bd7242bd7243be7145bf7047c06f49c16d4bc16c4dc26b50c36a52c46854c46756c56658c6655ac7645cc7625ec86160c96063c95f65ca5d67cb5c69cc5b6bcc5a6dcd586fce5771cf5673cf5576d05478d1527ad1517cd24f7fd24e82d34c84d44a87d4498ad5478cd5458fd64492d64294d74097d73f99d83d9cd83b9fd93aa1d938a4da36a7da35a9db33acdb31afdc30b1dc2eb4dd2cb6dd2bb9de29bcde27bedf26c1df26c3df26c6e026c8e026cbe026cde126d0e126d2e126d5e226d8e226dae226dde326dfe325e2e325e4e425e7e425e9e425ece525eee525f1e525f3e625f6e625f8e625fbe725fde725"},
};

// Parsed-once 256x3 RGB tables, keyed by name (lowercase).
inline const std::uint8_t *namedPalette(const std::string &name, int &n)
{
    static const std::map<std::string, std::vector<std::uint8_t>> tables = [] {
        std::map<std::string, std::vector<std::uint8_t>> m;
        for (const auto &p : kNamedPaletteHex) {
            std::vector<std::uint8_t> v;
            v.reserve(256 * 3);
            for (size_t i = 0; i + 1 < strlen(p.hex); i += 2)
                v.push_back((std::uint8_t)strtoul(std::string(p.hex, i, 2).c_str(),
                                        nullptr, 16));
            m[p.name] = std::move(v);
        }
        return m;
    }();
    std::string key;
    for (char c : name)
        key += (char)std::tolower((unsigned char)c);
    auto it = tables.find(key);
    if (it == tables.end())
        it = tables.find("parula");
    n = 256;
    return it->second.data();
}

// Custom palette from colormap(M) JSON "[[r,g,b],...]" (values [0,1]).
// figdetail::parseFlatDoubles stops at the first ']' (flat arrays), so
// brackets are stripped first and the numbers re-wrapped — "[[0,0,0],
// [1,0,0]]" → "[0,0,0,1,0,0]" — then grouped into RGB triplets.
inline std::vector<Rgb> parseCustomPalette(const std::string &json)
{
    std::string flat = "[";
    for (char ch : json)
        if (ch != '[' && ch != ']')
            flat += ch;
    flat += "]";
    auto v = figdetail::parseFlatDoubles(flat);
    std::vector<Rgb> out;
    out.reserve(v.size() / 3);
    for (size_t k = 0; k + 3 <= v.size(); k += 3)
        out.push_back(Rgb((std::uint8_t)std::lround(v[k] * 255),
                          (std::uint8_t)std::lround(v[k + 1] * 255),
                          (std::uint8_t)std::lround(v[k + 2] * 255)));
    return out;
}

// ── Display list ─────────────────────────────────────────────────────
struct Cmd {
    enum Kind { LINE, POLY, TEXT, MARK, IMAGE } kind;
    std::vector<double> xs, ys;   // LINE/POLY pixel points; MARK single;
                                  // IMAGE screen rect {x0,x1}/{y0,y1} of
                                  // the DATA rect [ix0,ix1]x[iy0,iy1]
    Rgb color;
    double width = 1.0;           // stroke width / marker size px
    bool fill = false;            // POLY: filled
    bool dash = false;            // LINE: dashed
    std::string text;             // TEXT
    double fontSize = 12;         // TEXT
    double x = 0, y = 0;          // TEXT baseline-left anchor / MARK centre
    bool rot90 = false;           // TEXT: rotate CCW (ylabel)
    bool bold = false;            // TEXT: FontWeight bold
    std::string fontFamily;       // TEXT: font name (SVG honours it;
                                  // the raster backend is monospace)

    // ── IMAGE (imagesc / image / imshow) ─────────────────────────────
    // Indexed form: img holds cell indices 0..254 with 255 = NaN
    // sentinel, COLUMN-major (idx = col*imgH + row) — the layout
    // DatasetInfo::zQuantized already uses. rgbDirect form (imshow
    // truecolor): img holds row-major RGBA QUADS (r,g,b,a — alpha is
    // synthesized 255 for plain RGB input, see image.cpp). qLo/qHi is
    // the baked quantization range; a cell's reconstructed value is
    // qLo + idx/254*(qHi-qLo). climLo/climHi is the display window
    // (CLim; auto = [qLo,qHi]).
    std::vector<std::uint8_t> img;
    int imgW = 0, imgH = 0;
    double ix0 = 0, ix1 = 1, iy0 = 0, iy1 = 1;
    double qLo = 0, qHi = 1;
    double climLo = 0, climHi = 1;
    std::string cmap = "parula";
    std::string cmapCustom;       // colormap(M) JSON, overrides cmap
    bool rgbDirect = false;
};

// Screen-space RGB buffer for an IMAGE cmd, shared by the raster and
// SVG backends so both produce identical pixels. The buffer size is the
// rounded on-screen rect (at least 1x1); each output pixel samples the
// containing cell via its centre (MATLAB-style nearest cell). Row/col 0
// sit at the screen end where the DATA rect's iy0/ix0 edge landed — the
// axes transform has already baked in yDir/xDir.
inline std::vector<std::uint8_t> imageCmdRgb(const Cmd &c)
{
    double sx0 = std::min(c.xs[0], c.xs[1]), sx1 = std::max(c.xs[0], c.xs[1]);
    double sy0 = std::min(c.ys[0], c.ys[1]), sy1 = std::max(c.ys[0], c.ys[1]);
    int pw = std::max(1, (int)std::lround(sx1 - sx0));
    int ph = std::max(1, (int)std::lround(sy1 - sy0));
    std::vector<std::uint8_t> buf((size_t)pw * ph * 3, 255);

    // u increases from the screen end of data edge ix0 toward ix1; same
    // for v with iy0. When the transform flipped the axis, the ix0 end
    // is the HIGH screen coordinate.
    auto frac = [](double lo, double hi, double v) {
        return hi > lo ? (v - lo) / (hi - lo) : 0.5;
    };
    std::vector<Rgb> custom;
    if (!c.cmapCustom.empty())
        custom = parseCustomPalette(c.cmapCustom);
    int nPal = 256;
    const std::uint8_t *pal = c.cmapCustom.empty()
        ? namedPalette(c.cmap, nPal) : nullptr;
    if (!custom.empty())
        nPal = (int)custom.size();

    for (int py = 0; py < ph; ++py) {
        double cy = sy0 + py + 0.5;
        double v = c.ys[0] <= c.ys[1] ? frac(sy0, sy1, cy)
                                      : 1.0 - frac(sy0, sy1, cy);
        int row = (int)(v * c.imgH);
        if (row < 0) row = 0;
        if (row >= c.imgH) row = c.imgH - 1;
        for (int px = 0; px < pw; ++px) {
            double cx = sx0 + px + 0.5;
            double u = c.xs[0] <= c.xs[1] ? frac(sx0, sx1, cx)
                                          : 1.0 - frac(sx0, sx1, cx);
            int col = (int)(u * c.imgW);
            if (col < 0) col = 0;
            if (col >= c.imgW) col = c.imgW - 1;
            Rgb rgb(255, 255, 255);
            if (c.rgbDirect) {
                size_t o = ((size_t)row * c.imgW + col) * 4;   // RGBA quad
                rgb = Rgb(c.img[o], c.img[o + 1], c.img[o + 2]);
            } else {
                std::uint8_t idx = c.img[(size_t)col * c.imgH + row];
                int e = 0;   // NaN sentinel and t<=0 both give entry 1
                if (idx != 255) {
                    double val = c.qLo + (idx / 254.0) * (c.qHi - c.qLo);
                    double t = (val - c.climLo) / (c.climHi - c.climLo);
                    if (t < 0) t = 0;
                    if (t > 1) t = 1;
                    e = (int)std::lround(t * (nPal - 1));
                    if (e < 0) e = 0;
                    if (e >= nPal) e = nPal - 1;
                }
                if (pal) {
                    rgb = Rgb(pal[e * 3], pal[e * 3 + 1], pal[e * 3 + 2]);
                } else {
                    rgb = custom[std::min((size_t)e, custom.size() - 1)];
                }
            }
            size_t o = ((size_t)py * pw + px) * 3;
            buf[o] = rgb.r;
            buf[o + 1] = rgb.g;
            buf[o + 2] = rgb.b;
        }
    }
    return buf;
}

// Nested 2-D JSON rows "[[a,b],[c,d]]" → row-major doubles + dims.
// Null entries (NaN cells) → NaN. Depth-tracked: only INNER brackets
// (depth 2) delimit rows — the outer pair is the wrapper (counting it
// as a row over-read the value vector).
inline void parseNestedDoubles(const std::string &j, std::vector<double> &out,
                               size_t &rows, size_t &cols)
{
    out.clear();
    rows = cols = 0;
    size_t i = 0;
    size_t colCount = 0;
    int depth = 0;
    while (i < j.size()) {
        char c = j[i];
        if (c == '[') {
            ++depth;
            if (depth == 2)
                colCount = 0;
            ++i;
        } else if (c == ']') {
            if (depth == 2) {
                ++rows;
                if (cols == 0)
                    cols = colCount;
            }
            --depth;
            ++i;
        } else if (c == ',' || c == ' ' || c == '\n' || c == '\t') {
            ++i;
        } else if (depth >= 2) {
            size_t e = i;
            while (e < j.size() && j[e] != ',' && j[e] != ']')
                ++e;
            std::string tok = j.substr(i, e - i);
            bool nullTok = tok.find("null") != std::string::npos;
            try {
                out.push_back(nullTok ? std::nan("") : std::stod(tok));
            } catch (...) {
                out.push_back(std::nan(""));
            }
            ++colCount;
            i = e;
        } else {
            ++i;
        }
    }
    if (!rows)
        out.clear();
}

// ── Ticks (ported from ide/src/components/plot/plotTicks.js) ─────────
// MATLAB-exact auto ticks (probe-verified R2025b, portion 34): target
// 10 intervals, ladder breakpoints 1/2/5 — [0,10]→step 1 (11 ticks),
// [0,3]→step 0.5 (7), [0,7]→step 1 (8).
inline std::vector<double> niceTicks(double mn, double mx, int target = 10)
{
    std::vector<double> major;
    double range = mx - mn;
    if (!(range > 0))
        return {mn};
    double rough = range / target;
    double pow = std::pow(10.0, std::floor(std::log10(rough)));
    double norm = rough / pow;
    double step = norm <= 1.0 ? pow
                : norm <= 2.0 ? 2 * pow
                : norm <= 5.0 ? 5 * pow : 10 * pow;
    for (double v = std::ceil(mn / step) * step; v <= mx + step * 1e-6; v += step)
        major.push_back(std::stod(std::to_string(v).substr(0, 12)));
    return major;
}

inline std::vector<double> logTicks(double mn, double mx)
{
    std::vector<double> major;
    if (!(mn > 0) || !(mx > mn))
        return major;
    int lmin = (int)std::floor(std::log10(mn));
    int lmax = (int)std::ceil(std::log10(mx));
    for (int p = lmin; p <= lmax; ++p) {
        double base = std::pow(10.0, p);
        if (base >= mn && base <= mx)
            major.push_back(base);
    }
    return major;
}

// Default tick label: %g-style with trailing-zero trim + exponent
// compaction ("1e+06" → "1e6").
inline std::string tickLabel(double v)
{
    if (v == 0)
        return "0";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    std::string s = buf;
    size_t e = s.find('e');
    if (e != std::string::npos) {
        std::string mant = s.substr(0, e);
        std::string exp = s.substr(e + 1);
        bool neg = !exp.empty() && exp[0] == '-';
        size_t k = 0;
        while (k < exp.size() && (exp[k] == '+' || exp[k] == '-'))
            ++k;
        while (k + 1 < exp.size() && exp[k] == '0')
            exp.erase(k, 1);
        if (mant.size() > 1 && mant.compare(mant.size() - 1, 1, "0") == 0
            && mant.find('.') != std::string::npos)
            while (mant.back() == '0')
                mant.pop_back();
        if (!mant.empty() && mant.back() == '.')
            mant.pop_back();
        s = mant + "e" + (neg ? "-" : "") + exp.substr(k);
    } else if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0')
            s.pop_back();
        if (!s.empty() && s.back() == '.')
            s.pop_back();
    }
    return s;
}

// ── Per-axes data range ──────────────────────────────────────────────
struct AxisRange {
    double lo = 0, hi = 1;
    bool log = false;
    bool valid() const { return hi > lo; }
    double map(double v, double p0, double p1) const   // data → pixel
    {
        double t;
        if (log) {
            double a = std::log10(lo), b = std::log10(hi);
            t = (std::log10(v) - a) / (b - a);
        } else {
            t = (v - lo) / (hi - lo);
        }
        return p0 + t * (p1 - p0);
    }
};

inline void dataRange(const std::vector<double> &v, double &lo, double &hi)
{
    lo = +INFINITY;
    hi = -INFINITY;
    for (double x : v)
        if (std::isfinite(x)) {
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
}

inline std::vector<double> parseVec(const std::string &json)
{
    return figdetail::parseFlatDoubles(json);
}

// ["a","b",…] tick-label json → strings.
inline std::vector<std::string> parseTickLabelVec(const std::string &json)
{
    std::vector<std::string> labels;
    size_t i = 0;
    while (i < json.size()) {
        size_t q1 = json.find('"', i);
        if (q1 == std::string::npos)
            break;
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos)
            break;
        labels.push_back(json.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return labels;
}

// ── Display-list builder ─────────────────────────────────────────────
struct Rect { double x0, y0, x1, y1; };   // pixel, y DOWN

inline void addLine(std::vector<Cmd> &out, std::vector<double> xs,
                    std::vector<double> ys, Rgb c, double w, bool dash)
{
    Cmd cmd;
    cmd.kind = Cmd::LINE;
    cmd.xs = std::move(xs);
    cmd.ys = std::move(ys);
    cmd.color = c;
    cmd.width = w;
    cmd.dash = dash;
    out.push_back(std::move(cmd));
}

inline void addText(std::vector<Cmd> &out, const std::string &s, double x,
                    double y, double size, Rgb c, bool rot90 = false,
                    bool bold = false, const std::string &family = "")
{
    if (s.empty())
        return;
    Cmd cmd;
    cmd.kind = Cmd::TEXT;
    cmd.text = s;
    cmd.x = x;
    cmd.y = y;
    cmd.fontSize = size;
    cmd.color = c;
    cmd.rot90 = rot90;
    cmd.bold = bold;
    cmd.fontFamily = family;
    out.push_back(std::move(cmd));
}

inline void addMarker(std::vector<Cmd> &out, double x, double y, double size,
                      Rgb c, const std::string &kind)
{
    Cmd cmd;
    cmd.kind = Cmd::MARK;
    cmd.x = x;
    cmd.y = y;
    cmd.width = size;
    cmd.color = c;
    cmd.text = kind;   // marker glyph selector
    cmd.fill = true;
    out.push_back(std::move(cmd));
}

inline void addPoly(std::vector<Cmd> &out, std::vector<double> xs,
                    std::vector<double> ys, Rgb c, bool fill)
{
    if (xs.size() < 2)
        return;
    Cmd cmd;
    cmd.kind = Cmd::POLY;
    cmd.xs = std::move(xs);
    cmd.ys = std::move(ys);
    cmd.color = c;
    cmd.fill = fill;
    cmd.width = 1;
    out.push_back(std::move(cmd));
}

// Render ONE axes into its pixel rect. `seriesIdxBase` continues the
// palette cycle across subplot cells (MATLAB restarts per axes; keep
// simple: per-axes reset).
inline void renderAxes(const FigureState &fig, const AxesState &ax, Rect box,
                       std::vector<Cmd> &out)
{
    const Rgb kAxisCol(0x40, 0x40, 0x44);
    const Rgb kGridCol(0xd8, 0xd8, 0xdc);
    const Rgb kTextCol(0x28, 0x28, 0x30);

    // Margin: room for tick labels + axis labels. axis off (`axis off`,
    // imshow) drops the decorations — a slim inset only, the image gets
    // the canvas (probe: imshow fills the saved figure).
    double ml = 64, mr = 14, mt = 20, mb = 48;
    if (!ax.title.empty())
        mt += 18;
    // Colorbar reserves space on its side (MATLAB eastoutside shrinks the
    // axes to ~0.68 width; we reserve strip + ticks + labels).
    std::string cbSide = ax.colorbarLocation;
    for (char &c : cbSide)
        c = (char)std::tolower((unsigned char)c);
    if (cbSide.size() > 7
        && cbSide.compare(cbSide.size() - 7, 7, "outside") == 0)
        cbSide.erase(cbSide.size() - 7);
    if (cbSide.empty() || cbSide == "best")
        cbSide = ax.colorbarLocation.empty() ? "" : "east";
    if (!cbSide.empty() && cbSide != "none") {
        if (cbSide == "east")
            mr += 48;
        else if (cbSide == "west")
            ml += 48;
        else if (cbSide == "south")
            mb += 48;
        else if (cbSide == "north")
            mt += 34;
    }
    Rect plot{box.x0 + ml, box.y0 + mt, box.x1 - mr, box.y1 - mb};
    if (!ax.axisVisible) {
        plot = Rect{box.x0 + 4, box.y0 + 4, box.x1 - 4, box.y1 - 4};
        if (!ax.title.empty())
            plot.y0 += 18;
    }

    // ── Collect series data → ranges ────────────────────────────────
    auto applyLim = [](const std::string &json, double &lo, double &hi) {
        if (json.empty())
            return;
        auto v = figdetail::parseFlatDoubles(json);
        if (v.size() == 2) {
            lo = v[0];
            hi = v[1];
        }
    };
    double xlo = +INFINITY, xhi = -INFINITY, ylo = +INFINITY, yhi = -INFINITY;
    std::vector<std::vector<double>> xs(ax.datasets.size()), ys(ax.datasets.size());
    int imgDatasets = 0, seriesDatasets = 0;

    // Image data rect (probe-verified MATLAB R2025b, portion 35). The
    // wire xJson/yJson are "[1,C]"/"[1,R]" ranges by default, a
    // per-column/row CENTER vector when numel==cells, or EDGE vectors
    // when numel==cells+1. Image edges: centers get ±spacing/2 (the
    // default [1,C] reduces to ±0.5); edge vectors are used as-is. When
    // the axes limits are auto, MATLAB pads the image extent by
    // span/(2*(cells-1)) where span = x(end)-x(1) of the GIVEN vector —
    // e.g. default 3x4 image → XLim [0.5 4.5]; centers [10..40] →
    // [5 45]; edges [10..50] → [3.333 56.667].
    auto imageEdges = [](const std::vector<double> &v, size_t cells,
                         double &e0, double &e1, double &pad) {
        if (v.size() < 2 || cells == 0) {
            e0 = 0.5;
            e1 = cells > 0 ? cells + 0.5 : 1.5;
            pad = 0.5;
            return;
        }
        double span = v.back() - v.front();
        if (v.size() == cells + 1) {         // edges as given
            e0 = v.front();
            e1 = v.back();
        } else {                             // centers (or [1,C] range)
            double sp = cells > 1 ? span / (double)(cells - 1) : (span > 0 ? span : 1.0);
            e0 = v.front() - sp / 2;
            e1 = v.back() + sp / 2;
        }
        pad = cells > 1 ? span / (2.0 * (double)(cells - 1)) : (span > 0 ? span / 2 : 0.5);
    };
    for (size_t i = 0; i < ax.datasets.size(); ++i) {
        const DatasetInfo &ds = ax.datasets[i];
        if (!ds.visible)
            continue;
        if (ds.type == "surf" || ds.type == "mesh" || ds.type == "waterfall"
            || (ds.type == "plot3" && !ds.zJson.empty())) {
            continue;   // rendered in the 3-D block above
        }
        if (ds.type == "imagesc" || ds.type == "image-rgb") {
            ++imgDatasets;
            size_t cols = ds.originalCols ? ds.originalCols : 1;
            size_t rows = ds.originalRows ? ds.originalRows : 1;
            double e0, e1, pd;
            imageEdges(parseVec(ds.xJson), cols, e0, e1, pd);
            xlo = std::min(xlo, e0 - pd);
            xhi = std::max(xhi, e1 + pd);
            imageEdges(parseVec(ds.yJson), rows, e0, e1, pd);
            ylo = std::min(ylo, e0 - pd);
            yhi = std::max(yhi, e1 + pd);
            continue;
        }
        if (ds.type == "text")
            continue;   // deferred: text layer
        ++seriesDatasets;
        xs[i] = parseVec(ds.xJson);
        ys[i] = parseVec(ds.yJson);
        if (xs[i].empty()) {
            xs[i].resize(ys[i].size());
            for (size_t k = 0; k < ys[i].size(); ++k)
                xs[i][k] = (double)(k + 1);
        }
        double a, b;
        dataRange(xs[i], a, b);
        xlo = std::min(xlo, a);
        xhi = std::max(xhi, b);
        dataRange(ys[i], a, b);
        ylo = std::min(ylo, a);
        yhi = std::max(yhi, a * 0 + b);
        // quiver: arrow tips extend the data range (u/v offsets).
        if (ds.type == "quiver") {
            auto uv = parseVec(ds.uJson);
            auto vv = parseVec(ds.vJson);
            const size_t nq = std::min(xs[i].size(), ys[i].size());
            for (size_t k = 0; k < nq && k < uv.size(); ++k) {
                xlo = std::min(xlo, xs[i][k] + uv[k]);
                xhi = std::max(xhi, xs[i][k] + uv[k]);
            }
            for (size_t k = 0; k < nq && k < vv.size(); ++k) {
                ylo = std::min(ylo, ys[i][k] + vv[k]);
                yhi = std::max(yhi, ys[i][k] + vv[k]);
            }
        }
        // errorbar extents
        auto ext = [&](const std::string &j, bool pos) {
            auto e = parseVec(j);
            if (e.empty())
                return;
            for (size_t k = 0; k < e.size() && k < ys[i].size(); ++k) {
                ylo = std::min(ylo, ys[i][k] - (pos ? 0 : e[k]));
                yhi = std::max(yhi, ys[i][k] + (pos ? e[k] : 0));
            }
        };
        ext(ds.eJson, true);
        ext(ds.eNegJson, false);
        ext(ds.ePosJson, true);
        if (ds.type == "stem") {
            ylo = std::min(ylo, 0.0);
            yhi = std::max(yhi, 0.0);
        }
    }
    if (!(xhi > xlo)) {
        xlo = 0;
        xhi = xhi > xlo ? xhi : xlo + 1;
    }
    if (!(yhi > ylo)) {
        double mid = ylo;
        ylo = mid - 0.5;
        yhi = mid + 0.5;
    }
    applyLim(ax.xlimJson, xlo, xhi);
    applyLim(ax.ylimJson, ylo, yhi);
    // 5% padding on non-explicit ranges (MATLAB-style breathing room).
    auto pad = [](double &lo, double &hi, const std::string &lim) {
        if (!lim.empty())
            return;
        double d = (hi - lo) * 0.05;
        lo -= d;
        hi += d;
    };
    // Image-only axes keep MATLAB's exact image limits (probe: XLim
    // [0.5 4.5] for a 3x4 — NO extra 5% breathing room). Mixed
    // image+series axes approximate MATLAB's union algorithm with the
    // usual 5% pad on top (rare in practice; noted in memory).
    bool imageOnly = imgDatasets > 0 && seriesDatasets == 0;
    if (!imageOnly) {
        pad(xlo, xhi, ax.xlimJson);
        pad(ylo, yhi, ax.ylimJson);
    }
    if (ax.xscale == "log") {
        xlo = std::max(xlo, 1e-12);
        if (!(xhi > xlo))
            xhi = xlo * 100;
    }
    if (ax.yscale == "log") {
        ylo = std::max(ylo, 1e-12);
        if (!(yhi > ylo))
            yhi = ylo * 100;
    }

    // axis image / axis equal — equal data units per pixel on both
    // axes: shrink the plot rect to the data aspect (MATLAB keeps the
    // box centered-ish; probe: imshow renders undistorted cells).
    if (ax.axisMode == "image" || ax.axisMode == "equal") {
        double dataAspect = (xhi - xlo) / (yhi - ylo);
        double pxAspect = (plot.x1 - plot.x0) / (plot.y1 - plot.y0);
        if (dataAspect > 0 && pxAspect > 0) {
            if (pxAspect > dataAspect) {
                double w = (plot.y1 - plot.y0) * dataAspect;
                double cx = (plot.x0 + plot.x1) / 2;
                plot.x0 = cx - w / 2;
                plot.x1 = cx + w / 2;
            } else {
                double h = (plot.x1 - plot.x0) / dataAspect;
                double cy = (plot.y0 + plot.y1) / 2;
                plot.y0 = cy - h / 2;
                plot.y1 = cy + h / 2;
            }
        }
    }

    AxisRange XR{xlo, xhi, ax.xscale == "log"};
    AxisRange YR{ylo, yhi, ax.yscale == "log"};
    double px0 = plot.x0, px1 = plot.x1;
    double py0 = plot.y1, py1 = plot.y0;   // y DOWN: data-up = pixel-up
    if (ax.xDir == "reverse")
        std::swap(px0, px1);
    if (ax.yDir == "reverse")
        std::swap(py0, py1);
    auto X = [&](double v) { return XR.map(v, px0, px1); };
    auto Y = [&](double v) { return YR.map(v, py0, py1); };

    // ── Grid (under the data) ───────────────────────────────────────
    if (ax.gridMajor && ax.gridUserTouched) {
        auto gx = XR.log ? logTicks(xlo, xhi) : niceTicks(xlo, xhi);
        auto gy = YR.log ? logTicks(ylo, yhi) : niceTicks(ylo, yhi);
        for (double v : gx)
            addLine(out, {X(v), X(v)}, {plot.y0, plot.y1}, kGridCol, 1, false);
        for (double v : gy)
            addLine(out, {plot.x0, plot.x1}, {Y(v), Y(v)}, kGridCol, 1, false);
    }

    // ── Images (under series — MATLAB draws lines over the image) ───
    for (size_t i = 0; i < ax.datasets.size(); ++i) {
        const DatasetInfo &ds = ax.datasets[i];
        if (!ds.visible || (ds.type != "imagesc" && ds.type != "image-rgb"))
            continue;
        size_t cols = ds.originalCols ? ds.originalCols : 1;
        size_t rows = ds.originalRows ? ds.originalRows : 1;
        double e0, e1, pd;
        imageEdges(parseVec(ds.xJson), cols, e0, e1, pd);
        double xe0 = e0, xe1 = e1;
        imageEdges(parseVec(ds.yJson), rows, e0, e1, pd);
        double ye0 = e0, ye1 = e1;
        Cmd c;
        c.kind = Cmd::IMAGE;
        if (ds.type == "imagesc") {
            c.img = ds.zQuantized;
            c.imgW = (int)cols;
            c.imgH = (int)rows;
            c.qLo = ds.cminOrig;
            c.qHi = ds.cmaxOrig;
            auto cl = figdetail::parseFlatDoubles(ax.climJson);
            if (cl.size() == 2 && cl[0] < cl[1]) {
                c.climLo = cl[0];
                c.climHi = cl[1];
            } else {
                c.climLo = c.qLo;   // CLim auto = data range
                c.climHi = c.qHi;
            }
            c.cmap = ax.colormapName.empty() ? "parula" : ax.colormapName;
            c.cmapCustom = ax.customColormapJson;
        } else {
            c.img = ds.rgbBytes;
            c.imgW = (int)cols;
            c.imgH = (int)rows;
            c.rgbDirect = true;
        }
        c.ix0 = xe0;
        c.ix1 = xe1;
        c.iy0 = ye0;
        c.iy1 = ye1;
        c.xs = {X(xe0), X(xe1)};
        c.ys = {Y(ye0), Y(ye1)};
        out.push_back(std::move(c));
    }

    // ── 3-D content (portion 40; MATLAB probes R2025b): surf = flat ──
    // per-quad faces coloured by the FIRST corner's Z through the
    // colormap (FaceColor 'flat', CData=Z), edges (0.129)x3; mesh /
    // waterfall = segments coloured by Z (EdgeColor 'flat'); view from
    // ax.viewJson, default (-37.5, 30); orthographic az/el projection;
    // painter's sort (far quads first). clim auto = [min Z, max Z] —
    // same rule as imagesc. The 3-D box replaces the 2-D frame.
    {
        bool has3d = false;
        for (const auto &ds : ax.datasets)
            if (ds.visible
                && (ds.type == "surf" || ds.type == "mesh"
                    || ds.type == "waterfall"
                    || (ds.type == "plot3" && !ds.zJson.empty()))
                && !ds.zJson.empty())
                has3d = true;
        if (has3d) {
            // Bounds over all 3-D datasets.
            double xlo3 = 1e300, xhi3 = -1e300, ylo3 = 1e300,
                   yhi3 = -1e300, zlo3 = 1e300, zhi3 = -1e300;
            auto acc = [](double &lo, double &hi, double v) {
                if (!std::isfinite(v))
                    return;
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            };
            struct SurfGrid {
                std::vector<double> z;   // row-major R x C
                std::vector<double> gx;  // column coords
                std::vector<double> gy;  // row coords
                size_t r, c;
            };
            std::vector<SurfGrid> grids;
            for (const auto &ds : ax.datasets) {
                if (!ds.visible || ds.zJson.empty())
                    continue;
                if (ds.type == "surf" || ds.type == "waterfall") {
                    auto xv = parseVec(ds.xJson);
                    auto yv = parseVec(ds.yJson);
                    std::vector<double> zf;
                    size_t r = 0, c = 0;
                    parseNestedDoubles(ds.zJson, zf, r, c);
                    if (xv.empty() || yv.empty() || !r || !c)
                        continue;
                    SurfGrid g;
                    g.z = std::move(zf);
                    for (size_t cc = 0; cc < c; ++cc)
                        g.gx.push_back(xv[std::min(cc, xv.size() - 1)]);
                    for (size_t rr = 0; rr < r; ++rr)
                        g.gy.push_back(yv[std::min(rr, yv.size() - 1)]);
                    g.r = r;
                    g.c = c;
                    for (size_t rr = 0; rr < r; ++rr)
                        for (size_t cc = 0; cc < c; ++cc) {
                            acc(xlo3, xhi3, g.gx[cc]);
                            acc(ylo3, yhi3, g.gy[rr]);
                            acc(zlo3, zhi3, g.z[rr * c + cc]);
                        }
                    grids.push_back(std::move(g));
                } else if (ds.type == "mesh" || ds.type == "plot3") {
                    auto xv = parseVec(ds.xJson);
                    auto yv = parseVec(ds.yJson);
                    auto zv = parseVec(ds.zJson);
                    size_t n = std::min(std::min(xv.size(), yv.size()),
                                        zv.size());
                    for (size_t k = 0; k < n; ++k) {
                        acc(xlo3, xhi3, xv[k]);
                        acc(ylo3, yhi3, yv[k]);
                        acc(zlo3, zhi3, zv[k]);
                    }
                }
            }
            if (!(xhi3 > xlo3)) { xlo3 = 0; xhi3 = 1; }
            if (!(yhi3 > ylo3)) { ylo3 = 0; yhi3 = 1; }
            if (!(zhi3 > zlo3)) { zlo3 = 0; zhi3 = 1; }
            applyLim(ax.xlimJson, xlo3, xhi3);
            applyLim(ax.ylimJson, ylo3, yhi3);
            applyLim(ax.zlimJson, zlo3, zhi3);
            // CLim + palette (same resolution as imagesc/colorbar).
            double clo3 = zlo3, chi3 = zhi3;
            {
                auto cl = figdetail::parseFlatDoubles(ax.climJson);
                if (cl.size() == 2 && cl[0] < cl[1]) {
                    clo3 = cl[0];
                    chi3 = cl[1];
                }
            }
            std::vector<Rgb> custom3;
            if (!ax.customColormapJson.empty())
                custom3 = parseCustomPalette(ax.customColormapJson);
            int nPal3 = 256;
            const std::uint8_t *pal3 =
                custom3.empty()
                    ? namedPalette(ax.colormapName.empty()
                                       ? std::string("parula")
                                       : ax.colormapName, nPal3)
                    : nullptr;
            if (!custom3.empty())
                nPal3 = (int)custom3.size();
            auto zColor = [&](double z) -> Rgb {
                if (!std::isfinite(z))
                    z = clo3;
                double t = (z - clo3) / (chi3 - clo3);
                if (t < 0) t = 0;
                if (t > 1) t = 1;
                int e = (int)std::lround(t * (nPal3 - 1));
                if (e < 0) e = 0;
                if (e >= nPal3) e = nPal3 - 1;
                return pal3 ? Rgb(pal3[e * 3], pal3[e * 3 + 1],
                                   pal3[e * 3 + 2])
                            : custom3[(size_t)e];
            };
            // View (az, el) in degrees; MATLAB orthographic transform.
            double azDeg = -37.5, elDeg = 30;
            {
                auto vv = figdetail::parseFlatDoubles(ax.viewJson);
                if (vv.size() == 2) {
                    azDeg = vv[0];
                    elDeg = vv[1];
                }
            }
            const double az = azDeg * 3.14159265358979323846 / 180.0;
            const double el = elDeg * 3.14159265358979323846 / 180.0;
            const double sAz = std::sin(az), cAz = std::cos(az);
            const double sEl = std::sin(el), cEl = std::cos(el);
            auto norm3 = [&](double v, double lo, double hi) {
                return hi > lo ? (v - lo) / (hi - lo) - 0.5 : 0.0;
            };
            // (sx, sy) in MATLAB screen space (y up), empirically
            // pinned by the corner-blob orientation test (view [0 90],
            // hot corner at data (x_min, y_min) renders LEFT-BOTTOM in
            // MATLAB R2025b): sx = -x*sin + y*cos; sy = (x*cos +
            // y*sin)*sin(el) + z*cos(el). Depth toward the camera =
            // -forward . p with forward = right x up (painter's sort,
            // larger = nearer).
            auto proj = [&](double x, double y, double z,
                            double &sx, double &sy, double &depth) {
                double xn = norm3(x, xlo3, xhi3);
                double yn = norm3(y, ylo3, yhi3);
                double zn = norm3(z, zlo3, zhi3);
                sx = -xn * sAz + yn * cAz;
                sy = (xn * cAz + yn * sAz) * sEl + zn * cEl;
                depth = zn * sEl - cEl * (xn * cAz + yn * sAz);
            };
            // Fit the projected unit cube into the plot rect.
            double minSx = 1e300, maxSx = -1e300, minSy = 1e300,
                   maxSy = -1e300;
            for (int corner = 0; corner < 8; ++corner) {
                double sx, sy, d;
                proj(corner & 1 ? xhi3 : xlo3, corner & 2 ? yhi3 : ylo3,
                     corner & 4 ? zhi3 : zlo3, sx, sy, d);
                minSx = std::min(minSx, sx);
                maxSx = std::max(maxSx, sx);
                minSy = std::min(minSy, sy);
                maxSy = std::max(maxSy, sy);
            }
            double spanX = std::max(1e-9, maxSx - minSx);
            double spanY = std::max(1e-9, maxSy - minSy);
            double availW = (plot.x1 - plot.x0) * 0.92;
            double availH = (plot.y1 - plot.y0) * 0.92;
            double sc = std::min(availW / spanX, availH / spanY);
            auto PX = [&](double sx) {
                return (plot.x0 + plot.x1) / 2
                     + (sx - (minSx + maxSx) / 2) * sc;
            };
            auto PY = [&](double sy) {
                return (plot.y0 + plot.y1) / 2
                     - (sy - (minSy + maxSy) / 2) * sc;
            };
            // Box: the 12 cube edges, projected.
            if (ax.axisVisible) {
                const double bx[2] = {xlo3, xhi3}, by[2] = {ylo3, yhi3},
                             bz[2] = {zlo3, zhi3};
                for (int axis = 0; axis < 3; ++axis)
                    for (int f1 = 0; f1 <= 1; ++f1)
                        for (int f2 = 0; f2 <= 1; ++f2) {
                            double q0[3], q1[3];
                            int o1 = (axis + 1) % 3, o2 = (axis + 2) % 3;
                            double fd0[3] = {0, 0, 0}, fd1[3] = {0, 0, 0};
                            fd0[axis] = 0; fd1[axis] = 1;
                            fd0[o1] = f1; fd1[o1] = f1;
                            fd0[o2] = f2; fd1[o2] = f2;
                            double w0[3] = {bx[(int)fd0[0]], by[(int)fd0[1]],
                                            bz[(int)fd0[2]]};
                            double w1[3] = {bx[(int)fd1[0]], by[(int)fd1[1]],
                                            bz[(int)fd1[2]]};
                            (void)q0; (void)q1;
                            double s0x, s0y, d0, s1x, s1y, d1;
                            proj(w0[0], w0[1], w0[2], s0x, s0y, d0);
                            proj(w1[0], w1[1], w1[2], s1x, s1y, d1);
                            addLine(out, {PX(s0x), PX(s1x)},
                                    {PY(s0y), PY(s1y)}, kAxisCol, 0.75,
                                    false);
                        }
            }
            // surf quads, painter's sort (far first).
            for (const auto &g : grids) {
                if (g.r < 2 || g.c < 2)
                    continue;   // single cell — no quads (mesh lines only)
                struct Q {
                    double px[4], py[4], depth;
                    Rgb col;
                };
                std::vector<Q> quads;
                for (size_t rr = 0; rr + 1 < g.r; ++rr)
                    for (size_t cc = 0; cc + 1 < g.c; ++cc) {
                        double z00 = g.z[rr * g.c + cc];
                        double z10 = g.z[(rr + 1) * g.c + cc];
                        double z01 = g.z[rr * g.c + cc + 1];
                        double z11 = g.z[(rr + 1) * g.c + cc + 1];
                        if (!std::isfinite(z00) || !std::isfinite(z10)
                            || !std::isfinite(z01) || !std::isfinite(z11))
                            continue;
                        double xs[4] = {g.gx[cc], g.gx[cc + 1], g.gx[cc + 1],
                                        g.gx[cc]};
                        double ys[4] = {g.gy[rr], g.gy[rr], g.gy[rr + 1],
                                        g.gy[rr + 1]};
                        double zs[4] = {z00, z01, z11, z10};
                        Q q;
                        q.depth = 0;
                        for (int k = 0; k < 4; ++k) {
                            double sx, sy, d;
                            proj(xs[k], ys[k], zs[k], sx, sy, d);
                            q.px[k] = PX(sx);
                            q.py[k] = PY(sy);
                            q.depth += d;
                        }
                        q.depth /= 4;
                        q.col = zColor(z00);   // FaceColor 'flat'
                        quads.push_back(q);
                    }
                std::sort(quads.begin(), quads.end(),
                          [](const Q &a, const Q &b) {
                              return a.depth < b.depth;
                          });
                const Rgb edge3(33, 33, 33);   // MATLAB 0.1294 gray
                for (const auto &q : quads) {
                    addPoly(out, {q.px[0], q.px[1], q.px[2], q.px[3]},
                            {q.py[0], q.py[1], q.py[2], q.py[3]}, q.col,
                            true);
                    if (ax.axisVisible)
                        addLine(out,
                                {q.px[0], q.px[1], q.px[2], q.px[3],
                                 q.px[0]},
                                {q.py[0], q.py[1], q.py[2], q.py[3],
                                 q.py[0]}, edge3, 0.75, false);
                }
            }
            // mesh / waterfall / plot3 polylines: segments; mesh and
            // waterfall coloured by the segment's first Z ('flat').
            for (const auto &ds : ax.datasets) {
                if (!ds.visible || ds.zJson.empty())
                    continue;
                if (ds.type != "mesh" && ds.type != "plot3"
                    && ds.type != "waterfall")
                    continue;
                if (ds.type == "waterfall") {
                    auto xv = parseVec(ds.xJson);
                    auto yv = parseVec(ds.yJson);
                    std::vector<double> zf;
                    size_t r, c;
                    parseNestedDoubles(ds.zJson, zf, r, c);
                    if (!r || !c)
                        continue;
                    for (size_t rr = 0; rr < r; ++rr)
                        for (size_t cc = 0; cc + 1 < c; ++cc) {
                            double z0 = zf[rr * c + cc];
                            double z1 = zf[rr * c + cc + 1];
                            if (!std::isfinite(z0) || !std::isfinite(z1))
                                continue;
                            double s0x, s0y, d0, s1x, s1y, d1;
                            proj(xv[std::min(cc, xv.size() - 1)],
                                 yv[std::min(rr, yv.size() - 1)], z0, s0x,
                                 s0y, d0);
                            proj(xv[std::min(cc + 1, xv.size() - 1)],
                                 yv[std::min(rr, yv.size() - 1)], z1, s1x,
                                 s1y, d1);
                            addLine(out, {PX(s0x), PX(s1x)},
                                    {PY(s0y), PY(s1y)}, zColor(z0), 1.2,
                                    false);
                        }
                    continue;
                }
                auto xv = parseVec(ds.xJson);
                auto yv = parseVec(ds.yJson);
                auto zv = parseVec(ds.zJson);
                size_t n = std::min(std::min(xv.size(), yv.size()),
                                    zv.size());
                StyleParts st3 = parseStyle(ds.style);
                for (size_t k = 0; k + 1 < n; ++k) {
                    if (!std::isfinite(xv[k]) || !std::isfinite(yv[k])
                        || !std::isfinite(zv[k]) || !std::isfinite(xv[k + 1])
                        || !std::isfinite(yv[k + 1])
                        || !std::isfinite(zv[k + 1]))
                        continue;
                    double s0x, s0y, d0, s1x, s1y, d1;
                    proj(xv[k], yv[k], zv[k], s0x, s0y, d0);
                    proj(xv[k + 1], yv[k + 1], zv[k + 1], s1x, s1y, d1);
                    Rgb col = ds.type == "mesh"
                                  ? zColor(zv[k])
                                  : (st3.hasColor ? st3.color
                                                  : Rgb(0x7f, 0xd9, 0x9a));
                    addLine(out, {PX(s0x), PX(s1x)}, {PY(s0y), PY(s1y)},
                            col, 1.2, false);
                }
            }
        }
    }

    // ── Series ──────────────────────────────────────────────────────
    size_t seriesIdx = 0;
    for (size_t i = 0; i < ax.datasets.size(); ++i) {
        const DatasetInfo &ds = ax.datasets[i];
        if (!ds.visible || xs[i].empty())
            continue;
        if (ds.type == "surf" || ds.type == "mesh" || ds.type == "waterfall"
            || (ds.type == "plot3" && !ds.zJson.empty()))
            continue;   // rendered in the 3-D block above
        StyleParts st = parseStyle(ds.style);
        if (!st.hasColor)
            st.color = palette()[seriesIdx % palette().size()];
        ++seriesIdx;
        Rgb col = st.color;
        double lw = ds.lineWidth > 0 ? ds.lineWidth : (st.lineStyle == "-" || st.lineStyle.empty() ? 1.4 : 1.4);
        double ms = ds.markerSize > 0 ? ds.markerSize * 1.4 : 6.0;
        bool dashed = st.lineStyle == "--" || st.lineStyle == ":" || st.lineStyle == "-.";
        const size_t n = std::min(xs[i].size(), ys[i].size());
        const std::string &t = ds.type;

        if (t == "line" || t == "comet" || t == "animatedline") {
            // Non-finite points (null separators — contour level
            // segments) SPLIT the polyline: dropping them would join
            // unrelated segments with spurious chords.
            std::vector<double> px, py;
            auto flushRun = [&]() {
                if (st.lineStyle != "none" && px.size() >= 2)
                    addLine(out, px, py, col, lw, dashed);
                px.clear();
                py.clear();
            };
            for (size_t k = 0; k < n; ++k) {
                if (std::isfinite(xs[i][k]) && std::isfinite(ys[i][k])) {
                    px.push_back(X(xs[i][k]));
                    py.push_back(Y(ys[i][k]));
                } else {
                    flushRun();
                }
            }
            flushRun();
            if (!st.marker.empty())
                for (size_t k = 0; k < px.size(); ++k)
                    addMarker(out, px[k], py[k], ms, col, st.marker);
        } else if (t == "stairs") {
            std::vector<double> px, py;
            for (size_t k = 0; k < n; ++k) {
                if (k && px.size()) {
                    px.push_back(X(xs[i][k]));
                    py.push_back(py.back());
                }
                px.push_back(X(xs[i][k]));
                py.push_back(Y(ys[i][k]));
            }
            if (px.size() >= 2)
                addLine(out, px, py, col, lw, dashed);
            if (!st.marker.empty())
                for (size_t k = 0; k < n; ++k)
                    addMarker(out, X(xs[i][k]), Y(ys[i][k]), ms, col, st.marker);
        } else if (t == "stem") {
            double base = 0;
            if (base >= YR.lo && base <= YR.hi)
                for (size_t k = 0; k < n; ++k)
                    addLine(out, {X(xs[i][k]), X(xs[i][k])},
                            {Y(base), Y(ys[i][k])}, col, lw, dashed);
            for (size_t k = 0; k < n; ++k)
                addMarker(out, X(xs[i][k]), Y(ys[i][k]), ms,
                          col, st.marker.empty() ? "o" : st.marker);
        } else if (t == "scatter" || t == "bubble") {
            std::vector<double> sz;
            if (!ds.sizeJson.empty())
                sz = parseVec(ds.sizeJson);
            for (size_t k = 0; k < n; ++k)
                addMarker(out, X(xs[i][k]), Y(ys[i][k]),
                          sz.empty() ? ms : std::max(3.0, std::sqrt(sz[k] / 3.14) * 1.6),
                          col, "o");
        } else if (t == "histogram") {
            // MATLAB histogram: bars TOUCH (full bin width), FaceColor
            // 'auto' = (0,0.447,0.741), light-gray edge, LW 0.5
            // (probe-verified R2025b). binWidth kv when the engine
            // packed it; otherwise derive from consecutive spacing.
            Rgb face = st.hasFaceColor ? st.faceColor : Rgb(0x00, 0x72, 0xbd);
            Rgb edge = st.edgeColor;
            if (st.faceAlpha < 1) {
                // MATLAB default FaceAlpha 0.6 composites with the
                // (white) canvas: 0.6·face + 0.4·white — probe-verified
                // mechanism (dark-theme renders were the SAME alpha over
                // a dark background, not a colour tint).
                double a = std::min(1.0, std::max(0.0, st.faceAlpha));
                face = Rgb((std::uint8_t)std::lround(a * face.r + (1 - a) * 255),
                           (std::uint8_t)std::lround(a * face.g + (1 - a) * 255),
                           (std::uint8_t)std::lround(a * face.b + (1 - a) * 255));
            }
            double bw = st.binWidth;
            if (!(bw > 0) && n > 1)
                bw = std::abs(xs[i][1] - xs[i][0]);
            for (size_t k = 0; k < n; ++k) {
                double x0p = X(xs[i][k] - bw / 2);
                double x1p = X(xs[i][k] + bw / 2);
                double yBase = Y(std::min(std::max(0.0, YR.lo), YR.hi));
                double yTop = Y(ys[i][k]);
                addPoly(out, {x0p, x1p, x1p, x0p},
                        {yBase, yBase, yTop, yTop}, face, true);
                addLine(out, {x0p, x1p}, {yTop, yTop}, edge, 0.75, false);
            }
        } else if (t == "bar") {
            // Grouped vertical bars, width 0.8 of slot.
            const int groups = (int)n;
            const int series = 1;   // per-dataset grouping (adjacent bar
                                    // datasets share slots below)
            for (size_t k = 0; k < n; ++k) {
                double slot = groups > 1 ? (xhi - xlo) / (groups - 1) : 1;
                double w = std::min(slot * 0.8, (plot.x1 - plot.x0) * 0.05) / series;
                double x0p = X(xs[i][k]) - w / 2;
                double yBase = Y(std::min(std::max(0.0, YR.lo), YR.hi));
                double yTop = Y(ys[i][k]);
                addPoly(out, {x0p, x0p + w, x0p + w, x0p},
                        {yBase, yBase, yTop, yTop}, col, true);
            }
        } else if (t == "quiver") {
            // Arrows: shaft (x,y)→(x+u,y+v) plus a chevron head at the
            // tip (two strokes ~25° back, length = max(4px, 12% of the
            // on-screen arrow length). NaN breaks skip the arrow.
            auto uv = parseVec(ds.uJson);
            auto vv = parseVec(ds.vJson);
            for (size_t k = 0; k < n; ++k) {
                double u = k < uv.size() ? uv[k] : 0.0;
                double v = k < vv.size() ? vv[k] : 0.0;
                if (!std::isfinite(xs[i][k]) || !std::isfinite(ys[i][k])
                    || !std::isfinite(u) || !std::isfinite(v))
                    continue;
                double x0 = X(xs[i][k]), y0 = Y(ys[i][k]);
                double x1 = X(xs[i][k] + u), y1 = Y(ys[i][k] + v);
                addLine(out, {x0, x1}, {y0, y1}, col, 1.2, false);
                double dx = x1 - x0, dy = y1 - y0;
                double len = std::hypot(dx, dy);
                if (len < 1e-9)
                    continue;
                double hl = std::max(4.0, len * 0.12);
                double ux = dx / len, uy = dy / len;
                // Back-left / back-right head strokes.
                double a1 = 0.45;   // ~26 degrees off the shaft
                double c1 = std::cos(a1), s1 = std::sin(a1);
                double bxl = x1 - hl * (ux * c1 - uy * s1);
                double byl = y1 - hl * (uy * c1 + ux * s1);
                double bxr = x1 - hl * (ux * c1 + uy * s1);
                double byr = y1 - hl * (uy * c1 - ux * s1);
                addLine(out, {bxl, x1}, {byl, y1}, col, 1.2, false);
                addLine(out, {bxr, x1}, {byr, y1}, col, 1.2, false);
            }
        } else if (t == "polygon") {
            // Filled rings (contourf bands, patch) — null-separated in
            // one dataset; each ring fills with the dataset colour.
            std::vector<double> px, py;
            auto flushRing = [&]() {
                if (px.size() >= 3)
                    addPoly(out, px, py, col, true);
                px.clear();
                py.clear();
            };
            for (size_t k = 0; k < n; ++k) {
                if (std::isfinite(xs[i][k]) && std::isfinite(ys[i][k])) {
                    px.push_back(X(xs[i][k]));
                    py.push_back(Y(ys[i][k]));
                } else {
                    flushRing();
                }
            }
            flushRing();
        } else if (t == "area") {
            std::vector<double> px, py;
            double yBase = Y(std::min(std::max(0.0, YR.lo), YR.hi));
            px.push_back(X(xs[i].front()));
            py.push_back(yBase);
            for (size_t k = 0; k < n; ++k) {
                px.push_back(X(xs[i][k]));
                py.push_back(Y(ys[i][k]));
            }
            px.push_back(X(xs[i][n - 1]));
            py.push_back(yBase);
            addPoly(out, px, py, col, true);
        } else if (t == "errorbar") {
            auto sym = parseVec(ds.eJson);
            auto eneg = parseVec(ds.eNegJson);
            auto epos = parseVec(ds.ePosJson);
            for (size_t k = 0; k < n; ++k) {
                double loV = ys[i][k], hiV = ys[i][k];
                if (k < sym.size()) {
                    loV -= sym[k];
                    hiV += sym[k];
                }
                if (k < eneg.size())
                    loV = ys[i][k] - eneg[k];
                if (k < epos.size())
                    hiV = ys[i][k] + epos[k];
                double pxk = X(xs[i][k]);
                addLine(out, {pxk, pxk}, {Y(loV), Y(hiV)}, col, 1, false);
                double cap = 4;
                addLine(out, {pxk - cap, pxk + cap}, {Y(loV), Y(loV)}, col, 1, false);
                addLine(out, {pxk - cap, pxk + cap}, {Y(hiV), Y(hiV)}, col, 1, false);
            }
            if (!st.marker.empty())
                for (size_t k = 0; k < n; ++k)
                    addMarker(out, X(xs[i][k]), Y(ys[i][k]), ms, col, st.marker);
        }
        // Unhandled types (quiver/feather/pie/…) silently skipped — see
        // the portion-32 export-gaps bug.
    }

    // ── text() annotations (portion 38; drawn OVER the series like ──
    // MATLAB). Defaults probe-verified R2025b: FontSize 10,
    // HorizontalAlignment LEFT, VerticalAlignment middle, black.
    for (size_t i = 0; i < ax.datasets.size(); ++i) {
        const DatasetInfo &ds = ax.datasets[i];
        if (!ds.visible || ds.type != "text")
            continue;
        auto xv = parseVec(ds.xJson);
        auto yv = parseVec(ds.yJson);
        if (xv.empty() || yv.empty())
            continue;
        TextExtras te = parseTextExtras(ds.style);
        double scale = std::max(1.0, (double)std::lround(te.fontSize / 7.0));
        double w = 6.0 * scale * ds.label.size();
        double x = X(xv[0]) - (te.halign == 1 ? w / 2
                          : te.halign == 2 ? w : 0.0);
        // Raster text() anchors at the glyph-box TOP; middle alignment
        // lifts the anchor by half the glyph height.
        double y = Y(yv[0]) - 3.5 * scale;
        addText(out, ds.label, x, y, te.fontSize, te.color, false, false,
                "");
    }

    // ── Box + ticks + labels (portion 34: custom ticks/labels, fonts,
    // frame width — all MATLAB-probe-verified). `axis off` (imshow,
    // `axis off`) skips every decoration except the title. ───────────
    const double boxW = std::max(0.75, ax.lineWidth);
    const double tickFS = ax.fontSize;
    const double labelFS = ax.fontSize * 1.1;
    const bool fBold = ax.fontWeight == "bold";
    const std::string &fam = ax.fontName;
    if (ax.axisVisible) {
    addLine(out, {plot.x0, plot.x1, plot.x1, plot.x0, plot.x0},
            {plot.y0, plot.y0, plot.y1, plot.y1, plot.y0}, kAxisCol, boxW,
            false);
    auto tx = !ax.xTicksJson.empty()
                  ? parseVec(ax.xTicksJson)
                  : (XR.log ? logTicks(xlo, xhi) : niceTicks(xlo, xhi));
    auto ty = !ax.yTicksJson.empty()
                  ? parseVec(ax.yTicksJson)
                  : (YR.log ? logTicks(ylo, yhi) : niceTicks(ylo, yhi));
    std::vector<std::string> txLab, tyLab;
    if (!ax.xTickLabelsJson.empty())
        txLab = parseTickLabelVec(ax.xTickLabelsJson);
    if (!ax.yTickLabelsJson.empty())
        tyLab = parseTickLabelVec(ax.yTickLabelsJson);
    for (size_t k = 0; k < tx.size(); ++k) {
        const double v = tx[k];
        double p = X(v);
        addLine(out, {p, p}, {plot.y1, plot.y1 + 4}, kAxisCol, boxW, false);
        std::string lab = k < txLab.size()
                              ? txLab[k]
                              : tickLabel(v);
        if (k >= txLab.size() && !ax.xTickFormat.empty()
            && ax.xTickFormat.size() > 1 && ax.xTickFormat[0] == '%') {
            char b[40];
            std::snprintf(b, sizeof b, ax.xTickFormat.c_str(), v);
            lab = b;
        }
        addText(out, lab, p - 3.0 * lab.size(), plot.y1 + 8, tickFS,
                kTextCol, false, fBold, fam);
    }
    for (size_t k = 0; k < ty.size(); ++k) {
        const double v = ty[k];
        double p = Y(v);
        addLine(out, {plot.x0 - 4, plot.x0}, {p, p}, kAxisCol, boxW, false);
        std::string lab = k < tyLab.size()
                              ? tyLab[k]
                              : tickLabel(v);
        if (k >= tyLab.size() && !ax.yTickFormat.empty()
            && ax.yTickFormat.size() > 1 && ax.yTickFormat[0] == '%') {
            char b[40];
            std::snprintf(b, sizeof b, ax.yTickFormat.c_str(), v);
            lab = b;
        }
        double w = 6.0 * lab.size();
        addText(out, lab, plot.x0 - 8 - w, p + 4, tickFS, kTextCol, false,
                fBold, fam);
    }
    if (!ax.xlabel.empty())
        addText(out, ax.xlabel, (plot.x0 + plot.x1) / 2 - 3.2 * ax.xlabel.size(),
                plot.y1 + 34, labelFS, kTextCol, false, fBold, fam);
    if (!ax.ylabel.empty())
        addText(out, ax.ylabel, plot.x0 - 46,
                (plot.y0 + plot.y1) / 2 + 3.2 * ax.ylabel.size(), labelFS,
                kTextCol, /*rot90=*/true, fBold, fam);
    }   // end axisVisible decorations
    if (!ax.title.empty())
        addText(out, ax.title, (plot.x0 + plot.x1) / 2 - 3.2 * ax.title.size(),
                plot.y0 - 18, labelFS + 2, kTextCol, false, fBold, fam);

    // ── Colorbar (portion 38; probe-verified R2025b: limits = CLim, ──
    // vertical ticks = the 10-interval ladder (12 for [1 12]), a
    // horizontal strip uses the 5-interval ladder, FontSize 9, strip
    // low end at bottom/left).
    if (!cbSide.empty() && cbSide != "none") {
        double lo = 0, hi = 1;
        auto cl = figdetail::parseFlatDoubles(ax.climJson);
        if (cl.size() == 2 && cl[0] < cl[1]) {
            lo = cl[0];
            hi = cl[1];
        } else {
            for (const auto &ds : ax.datasets)
                if (!ds.zQuantized.empty()) {
                    lo = ds.cminOrig;
                    hi = ds.cmaxOrig;
                    break;
                }
        }
        std::vector<Rgb> customPal;
        if (!ax.customColormapJson.empty())
            customPal = parseCustomPalette(ax.customColormapJson);
        int nPal = 256;
        const std::uint8_t *pal =
            customPal.empty()
                ? namedPalette(ax.colormapName.empty()
                                   ? std::string("parula")
                                   : ax.colormapName,
                               nPal)
                : nullptr;
        auto entryColor = [&](double t) -> Rgb {
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            int e = (int)std::lround(t * (nPal - 1));
            if (e < 0) e = 0;
            if (e >= nPal) e = nPal - 1;
            if (pal)
                return Rgb(pal[e * 3], pal[e * 3 + 1], pal[e * 3 + 2]);
            return customPal[(size_t)e];
        };
        const double cbFS = 9;      // probe: colorbar FontSize 9
        const double thick = 14;
        bool vert = cbSide == "east" || cbSide == "west";
        double sx, sy, sw, sh;
        if (vert) {
            sx = cbSide == "west" ? plot.x0 - 8 - thick : plot.x1 + 8;
            sy = plot.y0;
            sw = thick;
            sh = plot.y1 - plot.y0;
        } else {
            sx = plot.x0;
            sw = plot.x1 - plot.x0;
            sh = thick;
            sy = cbSide == "north" ? plot.y0 - 8 - thick : plot.y1 + 8;
        }
        int pw = std::max(1, (int)std::lround(sw));
        int ph = std::max(1, (int)std::lround(sh));
        std::vector<std::uint8_t> grad((size_t)pw * ph * 4, 255);
        for (int py = 0; py < ph; ++py)
            for (int px = 0; px < pw; ++px) {
                double t = vert ? 1.0 - (py + 0.5) / ph
                                : (px + 0.5) / pw;
                Rgb c = entryColor(t);
                size_t o = ((size_t)py * pw + px) * 4;
                grad[o] = c.r;
                grad[o + 1] = c.g;
                grad[o + 2] = c.b;
            }
        Cmd strip;
        strip.kind = Cmd::IMAGE;
        strip.rgbDirect = true;     // RGBA quads
        strip.imgW = pw;
        strip.imgH = ph;
        strip.img = std::move(grad);
        strip.xs = {sx, sx + pw};
        strip.ys = {sy, sy + ph};
        out.push_back(std::move(strip));
        // Frame around the strip (MATLAB draws a thin box).
        addLine(out, {sx, sx + sw, sx + sw, sx, sx},
                {sy, sy, sy + sh, sy + sh, sy}, kAxisCol, 0.75, false);
        auto ticks = niceTicks(lo, hi, vert ? 10 : 5);
        for (double v : ticks) {
            double t = (v - lo) / (hi - lo);
            if (t < -1e-9 || t > 1 + 1e-9)
                continue;
            std::string lab = tickLabel(v);
            double w = 6.0 * lab.size();
            if (vert) {
                double pyv = sy + sh - t * sh;
                bool west = cbSide == "west";
                addLine(out,
                        {west ? sx - 4 : sx + sw, west ? sx : sx + sw + 4},
                        {pyv, pyv}, kAxisCol, 0.75, false);
                addText(out, lab, west ? sx - 7 - w : sx + sw + 7,
                        pyv - 3.5, cbFS, kTextCol);
            } else {
                double pxv = sx + t * sw;
                bool north = cbSide == "north";
                addLine(out, {pxv, pxv},
                        {north ? sy - 4 : sy + sh, north ? sy : sy + sh + 4},
                        kAxisCol, 0.75, false);
                addText(out, lab, pxv - w / 2,
                        north ? sy - 9 - 7 : sy + sh + 9, cbFS, kTextCol);
            }
        }
    }

    // ── Legend ──────────────────────────────────────────────────────
    if (!ax.legendLabels.empty() && !ax.legendHidden) {
        double rowH = 16, pad2 = 6;
        double lw2 = 22, tw = 0;
        for (auto &s2 : ax.legendLabels)
            tw = std::max(tw, 6.0 * s2.size());
        double wLegend = pad2 + lw2 + 6 + tw + pad2;
        double hLegend = pad2 + rowH * ax.legendLabels.size() + pad2;
        double lx = plot.x1 - 10 - wLegend, ly = plot.y0 + 10;
        if (ax.legendLocation.find("south") != std::string::npos)
            ly = plot.y1 - 10 - hLegend;
        if (ax.legendLocation.find("west") != std::string::npos)
            lx = plot.x0 + 10;
        if (ax.legendLocation.find("outside") != std::string::npos)
            lx = plot.x1 + 8;
        addPoly(out, {lx, lx + wLegend, lx + wLegend, lx},
                {ly, ly, ly + hLegend, ly + hLegend}, kAxisCol, false);
        for (size_t k = 0; k < ax.legendLabels.size(); ++k) {
            double ry = ly + pad2 + rowH * k + rowH / 2;
            Rgb c = palette()[k % palette().size()];
            for (size_t d = 0; d < ax.datasets.size(); ++d) {
                if (ax.datasets[d].label == ax.legendLabels[k]) {
                    StyleParts sp = parseStyle(ax.datasets[d].style);
                    if (sp.hasColor)
                        c = sp.color;
                    break;
                }
            }
            addLine(out, {lx + pad2, lx + pad2 + lw2}, {ry, ry}, c, 1.6, false);
            addText(out, ax.legendLabels[k], lx + pad2 + lw2 + 6, ry + 4, 11,
                    kTextCol);
        }
    }
}

// Full figure → display list. Subplot grid tiling; single axes uses the
// whole canvas.
inline std::vector<Cmd> renderFigureDisplay(const FigureState &fig, int W,
                                            int H)
{
    std::vector<Cmd> out;
    int rows = std::max(1, fig.subplotRows);
    int cols = std::max(1, fig.subplotCols);
    // Order axes by subplotIndex (1-based reading order) when gridded.
    std::vector<const AxesState *> ordered;
    for (int p = 1; p <= rows * cols; ++p)
        for (const auto &ax : fig.axes)
            if (ax.subplotIndex == p) {
                ordered.push_back(&ax);
                break;
            }
    if (ordered.empty())
        for (const auto &ax : fig.axes)
            ordered.push_back(&ax);
    for (size_t i = 0; i < ordered.size(); ++i) {
        int r = rows > 1 || cols > 1 ? (int)i / cols : 0;
        int c = rows > 1 || cols > 1 ? (int)i % cols : 0;
        Rect box;
        box.x0 = 10 + c * (W - 20.0) / cols + 6;
        box.x1 = 10 + (c + 1) * (W - 20.0) / cols - 6;
        box.y0 = 10 + r * (H - 20.0) / rows + 6;
        box.y1 = 10 + (r + 1) * (H - 20.0) / rows - 6;
        if (fig.subplotRows == 0) {
            box = Rect{10, 10, (double)W - 10, (double)H - 10};
        }
        if (!fig.superTitle.empty() && r == 0 && c == (cols - 1) / 2) {
            addText(out, fig.superTitle,
                    W / 2.0 - 3.4 * fig.superTitle.size(), 6, 13,
                    Rgb(0x28, 0x28, 0x30));
            box.y0 += 10;
        }
        renderAxes(fig, *ordered[i], box, out);
    }
    // ── Figure-level annotations (portion 41): normalized [0,1] figure
    // units mapped straight onto the canvas. MATLAB defaults: Color
    // (0.1294)^3, HeadSize 10 (chevron ~12% of length), textarrow text
    // at the TAIL.
    for (const auto &an : fig.annotations) {
        Rgb col = an.colorHex.empty() ? Rgb(33, 33, 33)
                                      : Rgb::fromHex(an.colorHex);
        double ax0 = an.x1 * W, ay0 = (1.0 - an.y1) * H;
        double ax1 = an.x2 * W, ay1 = (1.0 - an.y2) * H;
        if (an.type == "line" || an.type == "arrow"
            || an.type == "textarrow") {
            addLine(out, {ax0, ax1}, {ay0, ay1}, col, 0.75, false);
            if (an.type != "line") {
                double dx = ax1 - ax0, dy = ay1 - ay0;
                double len = std::hypot(dx, dy);
                if (len > 1e-9) {
                    double hl = std::max(4.0, len * 0.12);
                    double ux = dx / len, uy = dy / len;
                    double a1 = 0.45;
                    double c1 = std::cos(a1), s1 = std::sin(a1);
                    addLine(out,
                            {ax1 - hl * (ux * c1 - uy * s1), ax1},
                            {ay1 - hl * (uy * c1 + ux * s1), ay1},
                            col, 0.75, false);
                    addLine(out,
                            {ax1 - hl * (ux * c1 + uy * s1), ax1},
                            {ay1 - hl * (uy * c1 - ux * s1), ay1},
                            col, 0.75, false);
                }
            }
            if (an.type == "textarrow" && !an.text.empty()) {
                double w = 6.0 * (an.fontSize / 10.0) * an.text.size();
                addText(out, an.text, ax0 - w - 6, ay0 - 3.5,
                        an.fontSize, col);
            }
        } else if (an.type == "rectangle" || an.type == "textbox") {
            double x0p = ax0, y0p = ay1, x1p = ax1, y1p = ay0;
            if (!an.faceHex.empty()) {
                Rgb fc = Rgb::fromHex(an.faceHex);
                addPoly(out, {x0p, x1p, x1p, x0p}, {y0p, y0p, y1p, y1p},
                        fc, true);
            }
            addLine(out, {x0p, x1p, x1p, x0p, x0p},
                    {y0p, y0p, y1p, y1p, y0p}, col, 0.75, false);
            if (an.type == "textbox" && !an.text.empty())
                addText(out, an.text, x0p + 6, (y0p + y1p) / 2 - 3.5,
                        an.fontSize, Rgb(0x28, 0x28, 0x30));
        } else if (an.type == "ellipse") {
            double cx = (ax0 + ax1) / 2, cy = (ay0 + ay1) / 2;
            double rx = std::abs(ax1 - ax0) / 2, ry = std::abs(ay1 - ay0) / 2;
            std::vector<double> ex, ey;
            for (int k = 0; k <= 64; ++k) {
                double t = 2 * 3.14159265358979323846 * k / 64.0;
                ex.push_back(cx + rx * std::cos(t));
                ey.push_back(cy + ry * std::sin(t));
            }
            if (!an.faceHex.empty()) {
                Rgb fc = Rgb::fromHex(an.faceHex);
                addPoly(out, ex, ey, fc, true);
            }
            for (size_t k = 0; k + 1 < ex.size(); ++k)
                addLine(out, {ex[k], ex[k + 1]}, {ey[k], ey[k + 1]}, col,
                        0.75, false);
        }
    }
    return out;
}

// ── SVG backend ─────────────────────────────────────────────────────
// Embeds an on-screen RGB buffer (top-down, w×h×3) as an image payload
// for SVG (a data: URL). The figure layer stays free of codec code —
// the bundle export glue supplies a PNG+base64 implementation; without
// one, SVG export skips IMAGE cmds (raster export always renders them).
using SvgImageEmbedder =
    std::function<std::string(const std::vector<std::uint8_t> &, int, int)>;

inline std::string renderFigureSvg(const FigureState &fig, int W, int H,
                                   const SvgImageEmbedder &embed = {})
{
    std::vector<Cmd> cmds = renderFigureDisplay(fig, W, H);
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
    s += std::to_string(W) + "\" height=\"" + std::to_string(H)
       + "\" viewBox=\"0 0 " + std::to_string(W) + " " + std::to_string(H)
       + "\">\n<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    auto esc = [](const std::string &t) {
        std::string o;
        for (char ch : t) {
            if (ch == '&')
                o += "&amp;";
            else if (ch == '<')
                o += "&lt;";
            else if (ch == '>')
                o += "&gt;";
            else
                o += ch;
        }
        return o;
    };
    for (const Cmd &c : cmds) {
        if (c.kind == Cmd::IMAGE) {
            if (!embed)
                continue;   // no codec available — skip (PNG path renders)
            std::vector<std::uint8_t> buf = imageCmdRgb(c);
            int pw = std::max(1, (int)std::lround(std::max(c.xs[0], c.xs[1])
                                                  - std::min(c.xs[0], c.xs[1])));
            int ph = std::max(1, (int)std::lround(std::max(c.ys[0], c.ys[1])
                                                  - std::min(c.ys[0], c.ys[1])));
            s += "<image x=\""
               + std::to_string(std::lround(std::min(c.xs[0], c.xs[1])))
               + "\" y=\""
               + std::to_string(std::lround(std::min(c.ys[0], c.ys[1])))
               + "\" width=\"" + std::to_string(pw) + "\" height=\""
               + std::to_string(ph) + "\" preserveAspectRatio=\"none\" href=\""
               + embed(buf, pw, ph) + "\"/>\n";
        } else if (c.kind == Cmd::TEXT) {
            s += "<text x=\"" + std::to_string(c.x) + "\" y=\""
               + std::to_string(c.y) + "\" font-size=\""
               + std::to_string(c.fontSize)
               + "\" font-family=\""
               + (c.fontFamily.empty() ? std::string("monospace")
                                       : c.fontFamily)
               + "\"" + std::string(c.bold ? " font-weight=\"bold\"" : "")
               + " fill=\"" + c.color.hex() + "\"";
            if (c.rot90)
                s += " transform=\"rotate(-90 " + std::to_string(c.x) + " "
                   + std::to_string(c.y) + ")\"";
            s += ">" + esc(c.text) + "</text>\n";
        } else if (c.kind == Cmd::MARK) {
            double r = c.width / 2;
            if (c.text == "s" || c.text == "d") {
                s += "<rect x=\"" + std::to_string(c.x - r) + "\" y=\""
                   + std::to_string(c.y - r) + "\" width=\""
                   + std::to_string(c.width) + "\" height=\""
                   + std::to_string(c.width) + "\" fill=\""
                   + (c.text == "d" ? "none" : c.color.hex()) + "\" stroke=\""
                   + c.color.hex() + "\"/>\n";
            } else if (c.text == "+" || c.text == "x") {
                s += "<path d=\"M" + std::to_string(c.x - r) + " "
                   + std::to_string(c.y) + " L" + std::to_string(c.x + r) + " "
                   + std::to_string(c.y) + " M" + std::to_string(c.x) + " "
                   + std::to_string(c.y - r) + " L" + std::to_string(c.x)
                   + " " + std::to_string(c.y + r)
                   + (c.text == "x" ? " M" + std::to_string(c.x - r) + " "
                                        + std::to_string(c.y - r) + " L"
                                        + std::to_string(c.x + r) + " "
                                        + std::to_string(c.y + r) + " M"
                                        + std::to_string(c.x + r) + " "
                                        + std::to_string(c.y - r) + " L"
                                        + std::to_string(c.x - r) + " "
                                        + std::to_string(c.y + r)
                                    : "")
                   + "\" stroke=\"" + c.color.hex() + "\" stroke-width=\"1.2\" fill=\"none\"/>\n";
            } else {   // o . * ^ v < > p h → circle
                s += "<circle cx=\"" + std::to_string(c.x) + "\" cy=\""
                   + std::to_string(c.y) + "\" r=\"" + std::to_string(r)
                   + "\" fill=\"none\" stroke=\"" + c.color.hex()
                   + "\" stroke-width=\"1.4\"/>\n";
            }
        } else {
            // LINE / POLY → <polyline>/<polygon>
            std::string pts;
            for (size_t i = 0; i < c.xs.size(); ++i) {
                if (i)
                    pts += " ";
                char b[48];
                std::snprintf(b, sizeof b, "%.1f,%.1f", c.xs[i], c.ys[i]);
                pts += b;
            }
            if (c.kind == Cmd::POLY && c.fill) {
                s += "<polygon points=\"" + pts + "\" fill=\"" + c.color.hex()
                   + "\" stroke=\"none\"/>\n";
            } else {
                s += "<polyline points=\"" + pts + "\" fill=\"none\" stroke=\""
                   + c.color.hex() + "\" stroke-width=\""
                   + std::to_string(c.width) + "\"";
                if (c.dash)
                    s += " stroke-dasharray=\"7 5\"";
                s += "/>\n";
            }
        }
    }
    s += "</svg>\n";
    return s;
}

// ── RGB raster backend ───────────────────────────────────────────────
// Classic public-domain 5×7 bitmap font (column-encoded, LSB = top row),
// ASCII 0x20..0x7E.
inline const std::uint8_t *font5x7(char c)
{
    // Canonical public-domain 5x7 font (Adafruit glcdfont, chars 0x20..0x7E),
    // column-encoded, LSB = top row. Unsized so a dropped row fails to
    // compile instead of silently shifting every later glyph.
    static const std::uint8_t F[][5] = {
        {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
        {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
        {0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},{0x00,0x1C,0x22,0x41,0x00},
        {0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
        {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},
        {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
        {0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},
        {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
        {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},
        {0x00,0x40,0x34,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
        {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},{0x3E,0x41,0x5D,0x59,0x4E},
        {0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
        {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x41,0x51,0x73},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
        {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
        {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
        {0x26,0x49,0x49,0x49,0x32},{0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},
        {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
        {0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},{0x00,0x7F,0x41,0x41,0x41},
        {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x41,0x7F},{0x04,0x02,0x01,0x02,0x04},
        {0x40,0x40,0x40,0x40,0x40},{0x00,0x03,0x07,0x08,0x00},{0x20,0x54,0x54,0x78,0x40},
        {0x7F,0x28,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},{0x38,0x44,0x44,0x28,0x7F},
        {0x38,0x54,0x54,0x54,0x18},{0x00,0x08,0x7E,0x09,0x02},{0x18,0xA4,0xA4,0x9C,0x78},
        {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x40,0x3D,0x00},
        {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x78,0x04,0x78},
        {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0xFC,0x18,0x24,0x24,0x18},
        {0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x24},
        {0x04,0x04,0x3F,0x44,0x24},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
        {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x4C,0x90,0x90,0x90,0x7C},
        {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x77,0x00,0x00},
        {0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02}
    };
    static_assert(sizeof F / sizeof F[0] == 95,
                  "font5x7 must cover exactly ASCII 0x20..0x7E");
    if (c < 0x20 || c > 0x7E)
        c = '?';
    return F[c - 0x20];
}

class Raster
{
public:
    Raster(int w, int h) : w_(w), h_(h), px_(w * h * 3, 255) {}

    void px(int x, int y, Rgb c)
    {
        if (x < 0 || y < 0 || x >= w_ || y >= h_)
            return;
        size_t o = (size_t)(y * w_ + x) * 3;
        px_[o] = c.r;
        px_[o + 1] = c.g;
        px_[o + 2] = c.b;
    }

    // Thick-ish Bresenham with optional dashing (7 on / 5 off).
    void line(double x0, double y0, double x1, double y1, Rgb c, double wd,
              bool dash)
    {
        double len = std::hypot(x1 - x0, y1 - y0);
        if (len < 1e-9)
            return;
        int steps = (int)std::ceil(len * 2);
        double dashOn = 7, dashOff = 5;
        double run = 0;
        int side = (int)std::max(1.0, std::round(wd));
        for (int i = 0; i <= steps; ++i) {
            double t = (double)i / steps;
            double x = x0 + (x1 - x0) * t;
            double y = y0 + (y1 - y0) * t;
            if (dash) {
                double m = std::fmod(run, dashOn + dashOff);
                run += len / steps;
                if (m > dashOn)
                    continue;
            }
            for (int dx = 0; dx < side; ++dx)
                for (int dy = 0; dy < side; ++dy)
                    px((int)x + dx - side / 2, (int)y + dy - side / 2, c);
        }
    }

    void polygon(const std::vector<double> &xs, const std::vector<double> &ys,
                 Rgb c, bool fill)
    {
        if (xs.size() < 2)
            return;
        if (fill) {
            // Scanline fill (even-odd).
            double ymn = ys[0], ymx = ys[0];
            for (double y : ys) {
                ymn = std::min(ymn, y);
                ymx = std::max(ymx, y);
            }
            for (int y = (int)std::floor(ymn); y <= (int)std::ceil(ymx); ++y) {
                std::vector<double> xi;
                for (size_t i = 0; i < xs.size(); ++i) {
                    size_t j = (i + 1) % xs.size();
                    double ya = ys[i], yb = ys[j];
                    if ((y + 0.5 >= ya && y + 0.5 < yb)
                        || (y + 0.5 >= yb && y + 0.5 < ya))
                        xi.push_back(xs[i] + (y + 0.5 - ya) / (yb - ya)
                                          * (xs[j] - xs[i]));
                }
                std::sort(xi.begin(), xi.end());
                for (size_t k = 0; k + 1 < xi.size(); k += 2)
                    for (int x = (int)std::ceil(xi[k]); x <= (int)xi[k + 1]; ++x)
                        px(x, y, c);
            }
        } else {
            for (size_t i = 0; i + 1 < xs.size(); ++i)
                line(xs[i], ys[i], xs[i + 1], ys[i + 1], c, 1, false);
            line(xs.back(), ys.back(), xs.front(), ys.front(), c, 1, false);
        }
    }

    void marker(double x, double y, double size, Rgb c, const std::string &k)
    {
        int r = (int)std::max(2.0, size / 2);
        if (k == "s") {
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                    px((int)x + dx, (int)y + dy, c);
        } else if (k == "+" || k == "x") {
            for (int d = -r; d <= r; ++d) {
                px((int)x + d, (int)y, c);
                px((int)x, (int)y + d, c);
                if (k == "x") {
                    px((int)x + d, (int)y + d, c);
                    px((int)x + d, (int)y - d, c);
                }
            }
        } else {   // o . * ^ v < > p h → outline circle (+ rays for *)
            for (int a = 0; a < 360; a += 2) {
                double rad = a * 3.14159265358979 / 180.0;
                px((int)std::lround(x + r * std::cos(rad)),
                   (int)std::lround(y + r * std::sin(rad)), c);
            }
            if (k == "*")
                for (int d = -r - 1; d <= r + 1; ++d) {
                    px((int)x + d, (int)y, c);
                    px((int)x, (int)y + d, c);
                    px((int)x + d, (int)y + d, c);
                    px((int)x + d, (int)y - d, c);
                }
        }
    }

    // Monospace text, scale = fontSize / 7 (glyph 5×7 + 1 col spacing).
    // rot90 renders the run bottom-to-top (ylabel).
    void text(double x, double y, const std::string &s, double fontSize, Rgb c,
              bool rot90)
    {
        int scale = std::max(1, (int)std::lround(fontSize / 7.0));
        if (!rot90) {
            double cx = x;
            for (char ch : s) {
                const std::uint8_t *g = font5x7(ch);
                for (int col = 0; col < 5; ++col)
                    for (int row = 0; row < 7; ++row)
                        if (g[col] & (1 << row))
                            for (int sx = 0; sx < scale; ++sx)
                                for (int sy = 0; sy < scale; ++sy)
                                    px((int)cx + col * scale + sx,
                                       (int)y + row * scale + sy, c);
                cx += 6 * scale;
            }
        } else {
            // Glyph columns run UPWARD from (x,y); within a glyph rows
            // go left→right after rotation.
            double cy = y;
            for (auto it = s.rbegin(); it != s.rend(); ++it) {
                const std::uint8_t *g = font5x7(*it);
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (g[col] & (1 << row))
                            for (int sx = 0; sx < scale; ++sx)
                                for (int sy = 0; sy < scale; ++sy)
                                    px((int)x + (6 - row) * scale + sx,
                                       (int)cy - (5 - col) * scale - sy, c);
                cy -= 6 * scale;
            }
        }
    }

    // Blit a top-down RGB buffer (pw×ph×3) at pixel (x0,y0). Used by the
    // IMAGE cmd; the buffer is already at on-screen resolution.
    void image(int x0, int y0, const std::vector<std::uint8_t> &buf, int pw,
               int ph)
    {
        for (int y = 0; y < ph; ++y) {
            int dy = y0 + y;
            if (dy < 0 || dy >= h_)
                continue;
            for (int x = 0; x < pw; ++x) {
                int dx = x0 + x;
                if (dx < 0 || dx >= w_)
                    continue;
                size_t src = ((size_t)y * pw + x) * 3;
                size_t o = ((size_t)dy * w_ + dx) * 3;
                px_[o] = buf[src];
                px_[o + 1] = buf[src + 1];
                px_[o + 2] = buf[src + 2];
            }
        }
    }

    const std::vector<std::uint8_t> &bytes() const { return px_; }
    int width() const { return w_; }
    int height() const { return h_; }

private:
    int w_, h_;
    std::vector<std::uint8_t> px_;
};

// Render to a top-down RGB buffer (H×W×3 bytes).
inline std::vector<std::uint8_t> renderFigureRgb(const FigureState &fig,
                                                 int W, int H)
{
    Raster r(W, H);
    for (const Cmd &c : renderFigureDisplay(fig, W, H)) {
        switch (c.kind) {
        case Cmd::TEXT:
            r.text(c.x, c.y, c.text, c.fontSize, c.color, c.rot90);
            if (c.bold)   // fake bold: second strike one pixel over
                r.text(c.x + 1, c.y, c.text, c.fontSize, c.color, c.rot90);
            break;
        case Cmd::MARK:
            r.marker(c.x, c.y, c.width, c.color, c.text);
            break;
        case Cmd::POLY:
            r.polygon(c.xs, c.ys, c.color, c.fill);
            break;
        case Cmd::LINE:
            for (size_t i = 0; i + 1 < c.xs.size(); ++i)
                r.line(c.xs[i], c.ys[i], c.xs[i + 1], c.ys[i + 1], c.color,
                       c.width, c.dash);
            break;
        case Cmd::IMAGE: {
            std::vector<std::uint8_t> buf = imageCmdRgb(c);
            int pw = std::max(1, (int)std::lround(std::max(c.xs[0], c.xs[1])
                                                  - std::min(c.xs[0], c.xs[1])));
            int ph = std::max(1, (int)std::lround(std::max(c.ys[0], c.ys[1])
                                                  - std::min(c.ys[0], c.ys[1])));
            r.image((int)std::lround(std::min(c.xs[0], c.xs[1])),
                    (int)std::lround(std::min(c.ys[0], c.ys[1])), buf, pw, ph);
            break;
        }
        }
    }
    return r.bytes();
}

} // namespace figrender
} // namespace nk
