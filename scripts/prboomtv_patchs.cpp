// prboomtv_patchs: applies line based patches on top of the upstream prboom
// sources, so a change does not need a full copy of the file it touches.
//
// usage: prboomtv_patchs --source <upstream dir> --output <dir> <patch.c>...
//
// A patch file holds one or more hunks, written in documentation notation:
//
//   /**
//    * @patch libretro/libretro.c 287-290    replaces original lines 287..290
//    */
//   ...code...
//   /* @endpatch */
//
//   /** @patch libretro/libretro.c 64 */     inserts before original line 64
//   ...code...
//   /* @endpatch */
//
// The doc comment may carry any other description; code sharing a line with
// the opening comment's close or with the end marker belongs to the hunk.
// Everything outside hunks is ignored. Line numbers always refer to the
// pristine upstream file, no matter how many hunks touch it; hunks that
// overlap each other are rejected instead of silently clobbering one another.
//
// Every patched file is written to <output>/<path> with #line markers that
// point back to the upstream file and to the patch, so diagnostics land on
// the right source. The relative path of each generated file is printed on
// stdout, one per line.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace rv = std::views;

namespace {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Hunk {
    fs::path patch;                  // patch file the hunk came from
    std::size_t directive = 0;       // line of the @patch directive
    std::size_t start = 0;           // line of the first body line
    std::size_t first = 0;           // first original line
    std::optional<std::size_t> last; // last replaced line, none = insertion
    std::vector<std::string> body;

    [[nodiscard]] bool insertion() const { return !last.has_value(); }

    [[nodiscard]] std::string where() const
    {
        return std::format("{}:{}", patch.string(), directive);
    }

    [[nodiscard]] std::string span() const
    {
        return insertion() ? std::format("insertion at line {}", first)
                           : std::format("lines {}-{}", first, *last);
    }
};

// upstream path (generic, relative) -> hunks in declaration order
using Hunks = std::map<std::string, std::vector<Hunk>>;

std::vector<std::string> read_lines(const fs::path& path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in)
        throw Error{std::format("{}: error: cannot open file", path.string())};

    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (line.ends_with('\r'))
            line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

bool is_blank(std::string_view line)
{
    return std::ranges::all_of(line, [](unsigned char c) { return std::isspace(c); });
}

std::string_view trim(std::string_view text)
{
    const auto space = [](unsigned char c) { return std::isspace(c); };
    while (!text.empty() && space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && space(text.back()))
        text.remove_suffix(1);
    return text;
}

fs::path checked_target(const fs::path& patch, std::size_t lineno, const std::string& raw)
{
    const fs::path target = fs::path{raw}.lexically_normal();
    const bool escapes = std::ranges::any_of(target, [](const fs::path& part) { return part == ".."; });
    if (target.is_absolute() || escapes || target.empty())
        throw Error{std::format("{}:{}: error: '{}' must be relative to the upstream tree",
                                patch.string(), lineno, raw)};
    return target;
}

void parse_patch(const fs::path& patch, Hunks& hunks)
{
    static const std::regex directive{R"(@patch\s+(\S+)\s+(\d+)(?:\s*-\s*(\d+))?(?=\s|\*/|$))"};
    static const std::regex mentions_patch{R"(@patch\b)"};
    static const std::regex mentions_end{R"(@endpatch\b)"};
    static const std::regex end_marker{R"(/\*\s*@endpatch\s*\*/)"};

    const auto lines = read_lines(patch);
    const auto fail = [&](std::size_t lineno, std::string_view what) {
        return Error{std::format("{}:{}: error: {}", patch.string(), lineno, what)};
    };

    Hunk* current = nullptr;

    // feeds one line (or the tail of one) to the open hunk, closing it on the
    // end marker
    const auto feed = [&](std::string_view text, std::size_t lineno) {
        std::match_results<std::string_view::const_iterator> end;
        if (std::regex_search(text.begin(), text.end(), end, end_marker)) {
            const std::string_view before{text.begin(), end[0].first};
            if (!is_blank(before) && current->body.empty())
                current->start = lineno;
            if (!is_blank(before))
                current->body.emplace_back(before.substr(0, before.find_last_not_of(" \t") + 1));
            current = nullptr;
            return;
        }
        if (std::regex_search(text.begin(), text.end(), mentions_patch))
            throw fail(lineno, std::format("@patch inside the hunk opened at line {}, missing /* @endpatch */",
                                           current->directive));
        if (current->body.empty())
            current->start = lineno;
        current->body.emplace_back(text);
    };

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto lineno = index + 1;
        const std::string_view line = lines[index];

        if (current) {
            feed(line, lineno);
            continue;
        }

        const auto open = line.find("/**");
        if (open == std::string_view::npos) {
            if (std::regex_search(line.begin(), line.end(), mentions_end))
                throw fail(lineno, "@endpatch without a matching @patch");
            continue;
        }

        // gather the whole doc comment, which may span several lines
        std::string comment;
        auto close_index = index;
        auto close = std::string_view::npos;
        for (auto from = open + 3; close_index < lines.size(); ++close_index, from = 0) {
            const std::string_view text = lines[close_index];
            close = text.find("*/", from);
            comment.append(text.substr(from, close == std::string_view::npos ? text.npos : close - from));
            comment += '\n';
            if (close != std::string_view::npos)
                break;
        }
        if (close == std::string_view::npos)
            throw fail(lineno, "unterminated /** comment");

        std::smatch match;
        if (!std::regex_search(comment, match, directive)) {
            if (std::regex_search(comment, mentions_patch))
                throw fail(lineno, "malformed directive, expected '@patch <file> <line>[-<line>]'");
            index = close_index;
            continue;
        }

        const auto directive_line = lineno + static_cast<std::size_t>(
            std::ranges::count(comment.begin(), comment.begin() + match.position(0), '\n'));
        const auto target = checked_target(patch, directive_line, match[1]).generic_string();
        current = &hunks[target].emplace_back(Hunk{
            .patch = patch,
            .directive = directive_line,
            .start = close_index + 2,
            .first = std::stoul(match[2]),
            .last = match[3].matched ? std::optional{std::stoul(match[3])} : std::nullopt,
            .body = {},
        });

        // code right after the comment, on the same line, opens the body
        const auto rest = std::string_view{lines[close_index]}.substr(close + 2);
        if (!is_blank(rest))
            feed(trim(rest), close_index + 1);
        index = close_index;
    }

    if (current)
        throw fail(current->directive, "missing /* @endpatch */");
}

void validate(const std::string& target, const std::vector<Hunk>& hunks, std::size_t count)
{
    for (const auto& hunk : hunks) {
        const auto limit = hunk.insertion() ? count + 1 : count;
        const auto end = hunk.last.value_or(hunk.first);
        if (hunk.first == 0 || end < hunk.first || end > limit)
            throw Error{std::format("{}: error: {} is out of range, {} has {} lines",
                                    hunk.where(), hunk.span(), target, count)};
    }

    auto replaces = hunks
        | rv::filter([](const Hunk& h) { return !h.insertion(); })
        | rv::transform([](const Hunk& h) { return &h; })
        | std::ranges::to<std::vector>();
    std::ranges::sort(replaces, {}, &Hunk::first);

    for (const auto& [prev, next] : replaces | rv::adjacent<2>) {
        if (*prev->last >= next->first)
            throw Error{std::format("{}: error: {} overlaps {} from {}",
                                    next->where(), next->span(), prev->span(), prev->where())};
    }

    // an insertion may sit right before a replaced block, never inside it
    for (const auto& hunk : hunks | rv::filter(&Hunk::insertion)) {
        const auto inside = std::ranges::find_if(replaces, [&](const Hunk* r) {
            return r->first < hunk.first && hunk.first <= *r->last;
        });
        if (inside != replaces.end())
            throw Error{std::format("{}: error: {} falls inside {} from {}",
                                    hunk.where(), hunk.span(), (*inside)->span(), (*inside)->where())};
    }
}

std::string quoted(const fs::path& path)
{
    std::string out = "\"";
    for (const char c : fs::absolute(path).generic_string()) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out + '"';
}

std::string apply(const fs::path& original, const std::vector<std::string>& lines,
                  std::vector<Hunk> hunks)
{
    // insertions go before a replacement starting on the same line; ties keep
    // declaration order
    std::ranges::stable_sort(hunks, {}, [](const Hunk& h) {
        return std::pair{h.first, !h.insertion()};
    });

    std::ostringstream out;
    bool continued = false; // a #line can not break a backslash continuation
    const auto emit = [&](std::string_view line) {
        out << line << '\n';
        continued = line.ends_with('\\');
    };
    const auto mark = [&](std::size_t line, const fs::path& file) {
        if (!continued)
            out << std::format("#line {} {}\n", line, quoted(file));
    };

    mark(1, original);

    auto hunk = hunks.begin();
    std::size_t line = 1;
    while (line <= lines.size() + 1) {
        if (hunk != hunks.end() && hunk->first == line) {
            mark(hunk->start, hunk->patch);
            std::ranges::for_each(hunk->body, emit);
            if (!hunk->insertion())
                line = *hunk->last + 1;
            ++hunk;
            mark(line, original);
            continue;
        }
        if (line <= lines.size())
            emit(lines[line - 1]);
        ++line;
    }
    return out.str();
}

void write_if_changed(const fs::path& path, const std::string& content)
{
    if (fs::exists(path)) {
        std::ifstream in{path, std::ios::binary};
        const std::string current{std::istreambuf_iterator<char>{in}, {}};
        if (current == content)
            return;
    }
    fs::create_directories(path.parent_path());
    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    out << content;
    if (!out)
        throw Error{std::format("{}: error: cannot write file", path.string())};
}

void remove_stale(const fs::path& output, const std::set<fs::path>& keep)
{
    if (!fs::exists(output))
        return;
    const auto stale = fs::recursive_directory_iterator{output}
        | rv::filter([](const fs::directory_entry& e) { return e.is_regular_file(); })
        | rv::transform([](const fs::directory_entry& e) { return e.path(); })
        | rv::filter([&](const fs::path& p) { return !keep.contains(p); })
        | std::ranges::to<std::vector>();
    for (const auto& path : stale)
        fs::remove(path);
}

int run(std::span<char*> args)
{
    fs::path source;
    fs::path output;
    std::vector<fs::path> patches;

    for (auto arg = args.begin(); arg != args.end(); ++arg) {
        const std::string_view current{*arg};
        if ((current == "--source" || current == "--output") && std::next(arg) != args.end())
            (current == "--source" ? source : output) = *++arg;
        else if (current.starts_with("--"))
            throw Error{std::format("error: unknown option '{}'", current)};
        else
            patches.emplace_back(current);
    }
    if (source.empty() || output.empty())
        throw Error{"usage: prboomtv_patchs --source <upstream dir> --output <dir> <patch.c>..."};

    // deterministic order no matter how the shell or cmake listed them
    std::ranges::sort(patches);

    Hunks hunks;
    for (const auto& patch : patches)
        parse_patch(patch, hunks);

    std::set<fs::path> generated;
    for (const auto& [target, list] : hunks) {
        const auto original = source / target;
        if (!fs::is_regular_file(original))
            throw Error{std::format("{}: error: '{}' does not exist upstream",
                                    list.front().where(), target)};

        const auto lines = read_lines(original);
        validate(target, list, lines.size());

        const auto destination = output / target;
        write_if_changed(destination, apply(original, lines, list));
        generated.insert(destination);
        std::cout << target << '\n';
    }

    remove_stale(output, generated);
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
