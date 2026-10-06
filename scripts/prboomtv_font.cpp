// prboomtv_font: turns the Odamex big font into data the core embeds.
//
// usage: prboomtv_font --glyphs <dir> --palette <doom.gpl>
//                      --wad <dopo_wad_data.h> --font <dopo_font_data.h>
//
// Every glyphs/fontbNN.png (NN = ASCII code - 32, the Heretic FONTB naming
// Odamex uses) becomes a Doom patch lump DOPOFnnn (nnn = ASCII code) in a
// small PWAD, written as a C array (--wad). The glyphs are drawn with Doom's
// red ramp, so each color is mapped back to its palette index, preferring
// the ranges prboom's CR_* translation tables recolor; the font can then be
// drawn in any CR_ color at runtime. Font metrics go to --font. Outputs are
// only rewritten on change.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <regex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace rv = std::views;

namespace {

constexpr int first_char = 33;  // fontb01 = '!'
constexpr int space_width = 8;  // like Heretic's FONTB
constexpr int glyph_overlap = 1;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

using Bytes = std::vector<std::uint8_t>;

std::string read_file(const fs::path& path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in)
        throw Error{std::format("{}: error: cannot open file", path.string())};
    return {std::istreambuf_iterator<char>{in}, {}};
}

void write_if_changed(const fs::path& path, const std::string& content)
{
    if (fs::exists(path) && read_file(path) == content)
        return;
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    out << content;
    if (!out)
        throw Error{std::format("{}: error: cannot write file", path.string())};
}

void put16(Bytes& out, int value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

void put32(Bytes& out, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
}

struct Rgb {
    int r, g, b;
};

// GIMP palette: "R G B<tab>name" lines after the header
std::vector<Rgb> load_palette(const fs::path& path)
{
    static const std::regex entry{R"(^\s*(\d+)\s+(\d+)\s+(\d+)\b)"};
    std::istringstream in{read_file(path)};
    std::vector<Rgb> palette;
    for (std::string line; std::getline(in, line);) {
        std::smatch match;
        if (std::regex_search(line, match, entry))
            palette.push_back({std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3])});
    }
    if (palette.size() != 256)
        throw Error{std::format("{}: error: expected 256 colors, got {}", path.string(), palette.size())};
    return palette;
}

// the red ramp and dark reds the CR_* tables translate come first, so a
// color both ranges share is stored where it can be recolored
std::vector<int> search_order()
{
    std::vector<int> order;
    for (int i = 176; i <= 191; ++i)
        order.push_back(i);
    for (const int i : {44, 45, 47})
        order.push_back(i);
    for (int i = 0; i < 256; ++i)
        if (std::ranges::find(order, i) == order.end())
            order.push_back(i);
    return order;
}

int to_palette(const Rgb& color, const std::vector<Rgb>& palette, const std::vector<int>& order)
{
    const auto distance = [&](int index) {
        const auto& c = palette[index];
        const int dr = c.r - color.r, dg = c.g - color.g, db = c.b - color.b;
        return dr * dr + dg * dg + db * db;
    };
    return std::ranges::min(order, {}, distance);
}

struct Glyph {
    int code;
    Bytes patch;
};

// Doom patch: header, column offsets, then columns of opaque posts
Bytes encode_patch(int width, int height, std::span<const std::uint8_t> rgba,
                   const std::vector<Rgb>& palette, const std::vector<int>& order)
{
    const auto pixel = [&](int x, int y) { return rgba.subspan(static_cast<std::size_t>((y * width + x) * 4), 4); };
    const auto opaque = [&](int x, int y) { return pixel(x, y)[3] >= 128; };

    Bytes columns;
    std::vector<std::uint32_t> offsets;
    const auto header = 8 + 4 * width;

    for (int x = 0; x < width; ++x) {
        offsets.push_back(static_cast<std::uint32_t>(header + columns.size()));
        int y = 0;
        while (y < height) {
            if (!opaque(x, y)) {
                ++y;
                continue;
            }
            const int top = y;
            while (y < height && opaque(x, y))
                ++y;
            columns.push_back(static_cast<std::uint8_t>(top));
            columns.push_back(static_cast<std::uint8_t>(y - top));
            columns.push_back(0);
            for (int row = top; row < y; ++row) {
                const auto p = pixel(x, row);
                columns.push_back(static_cast<std::uint8_t>(to_palette({p[0], p[1], p[2]}, palette, order)));
            }
            columns.push_back(0);
        }
        columns.push_back(0xff);
    }

    Bytes out;
    put16(out, width);
    put16(out, height);
    put16(out, 0);
    put16(out, 0);
    for (const auto offset : offsets)
        put32(out, offset);
    std::ranges::copy(columns, std::back_inserter(out));
    return out;
}

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

Image load_png(const fs::path& path)
{
    int width = 0, height = 0, channels = 0;
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels{
        stbi_load(path.string().c_str(), &width, &height, &channels, 4), stbi_image_free};
    if (!pixels)
        throw Error{std::format("{}: error: {}", path.string(), stbi_failure_reason())};
    return {width, height, {pixels.get(), pixels.get() + width * height * 4}};
}

struct Font {
    std::vector<Glyph> glyphs;
    int height = 0;
};

Font load_glyphs(const fs::path& dir, const std::vector<Rgb>& palette)
{
    std::map<int, Image> images;
    for (int index = 1;; ++index) {
        const auto path = dir / std::format("fontb{:02}.png", index);
        if (!fs::exists(path))
            break;
        images[first_char + index - 1] = load_png(path);
    }
    if (images.empty())
        throw Error{std::format("{}: error: no fontbNN.png glyphs", dir.string())};

    const auto order = search_order();
    Font font;
    for (const auto& [code, image] : images) {
        font.glyphs.push_back({code, encode_patch(image.width, image.height, image.rgba, palette, order)});
        font.height = std::max(font.height, image.height);
    }
    return font;
}

Bytes build_wad(const std::vector<Glyph>& glyphs)
{
    Bytes lumps;
    Bytes directory;
    for (const auto& glyph : glyphs) {
        put32(directory, static_cast<std::uint32_t>(12 + lumps.size()));
        put32(directory, static_cast<std::uint32_t>(glyph.patch.size()));
        auto name = std::format("DOPOF{:03}", glyph.code);
        name.resize(8, '\0');
        std::ranges::copy(name, std::back_inserter(directory));
        std::ranges::copy(glyph.patch, std::back_inserter(lumps));
    }

    Bytes wad{'P', 'W', 'A', 'D'};
    put32(wad, static_cast<std::uint32_t>(glyphs.size()));
    put32(wad, static_cast<std::uint32_t>(12 + lumps.size()));
    std::ranges::copy(lumps, std::back_inserter(wad));
    std::ranges::copy(directory, std::back_inserter(wad));
    return wad;
}

std::string wad_header(const Bytes& wad)
{
    std::ostringstream out;
    out << "/* Generated by scripts/prboomtv_font.cpp from the Odamex big font. */\n"
        << "#ifndef DOPO_WAD_DATA_H\n#define DOPO_WAD_DATA_H\n\n"
        << "static const unsigned char dopo_wad_data[] = {";
    for (const auto& [index, byte] : wad | rv::enumerate)
        out << (index % 16 ? " " : "\n  ") << std::format("0x{:02x},", byte);
    out << "\n};\n"
        << std::format("static const unsigned int dopo_wad_data_len = {}u;\n\n#endif\n", wad.size());
    return out.str();
}

std::string font_header(const Font& font)
{
    return std::format(
        "/* Generated by scripts/prboomtv_font.cpp from the Odamex big font. */\n"
        "#ifndef DOPO_FONT_DATA_H\n#define DOPO_FONT_DATA_H\n\n"
        "/* glyphs DOPOF{:03}..DOPOF{:03}, uppercase only */\n"
        "#define DOPO_FONT_FIRST   {}\n"
        "#define DOPO_FONT_LAST    {}\n"
        "#define DOPO_FONT_SPACE   {}\n"
        "#define DOPO_FONT_OVERLAP {}\n"
        "#define DOPO_FONT_HEIGHT  {}\n\n#endif\n",
        font.glyphs.front().code, font.glyphs.back().code, font.glyphs.front().code,
        font.glyphs.back().code, space_width, glyph_overlap, font.height);
}

int run(std::span<char*> args)
{
    std::map<std::string, fs::path> options;
    for (std::size_t i = 0; i + 1 < args.size(); i += 2)
        options[args[i]] = args[i + 1];

    const auto need = [&](const std::string& key) {
        const auto found = options.find(key);
        if (found == options.end())
            throw Error{"usage: prboomtv_font --glyphs <dir> --palette <doom.gpl> --wad <out.h> --font <out.h>"};
        return found->second;
    };

    const auto font = load_glyphs(need("--glyphs"), load_palette(need("--palette")));
    write_if_changed(need("--wad"), wad_header(build_wad(font.glyphs)));
    write_if_changed(need("--font"), font_header(font));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return run(std::span{argv + 1, static_cast<std::size_t>(argc - 1)});
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
