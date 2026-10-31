#include "common/input_script.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

#include "core/bus/io.hpp"
#include "core/sched/machine.hpp"

namespace gen::common {

namespace {

std::vector<std::string_view> tokens(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
            ++i;
        }
        std::size_t j = i;
        while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j])) == 0) {
            ++j;
        }
        if (j > i) {
            out.push_back(line.substr(i, j - i));
        }
        i = j;
    }
    return out;
}

bool parseU64(std::string_view s, std::uint64_t& out) {
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && p == s.data() + s.size() && !s.empty();
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

bool isHash(std::string_view s) {
    return s.size() == 16 && std::all_of(s.begin(), s.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
}

}  // namespace

const char* toString(CheckKind kind) {
    switch (kind) {
        case CheckKind::FrameHash:
            return "frame_hash";
        case CheckKind::StateHash:
            return "state_hash";
        case CheckKind::AudioHash:
            return "audio_hash";
    }
    return "?";
}

std::uint64_t InputScript::lastFrame() const {
    std::uint64_t last = 0;
    for (const auto& e : events) {
        last = std::max(last, e.frame);
    }
    for (const auto& c : checkpoints) {
        last = std::max(last, c.frame);
    }
    return last;
}

std::optional<std::uint16_t> parseButtons(std::string_view text) {
    if (text == "-") {
        return std::uint16_t{0};
    }
    static constexpr std::pair<std::string_view, std::uint16_t> kNames[] = {
        {"UP", bus::kPadUp},       {"DOWN", bus::kPadDown}, {"LEFT", bus::kPadLeft}, {"RIGHT", bus::kPadRight},
        {"A", bus::kPadA},         {"B", bus::kPadB},       {"C", bus::kPadC},       {"START", bus::kPadStart},
        {"X", bus::kPadX},         {"Y", bus::kPadY},       {"Z", bus::kPadZ},       {"MODE", bus::kPadMode},
    };
    std::uint16_t mask = 0;
    const std::string u = upper(text);
    std::string_view rest = u;
    while (!rest.empty()) {
        const std::size_t plus = rest.find('+');
        const std::string_view name = rest.substr(0, plus);
        const auto* it = std::find_if(std::begin(kNames), std::end(kNames), [&](const auto& n) { return n.first == name; });
        if (it == std::end(kNames)) {
            return std::nullopt;
        }
        mask = static_cast<std::uint16_t>(mask | it->second);
        rest = plus == std::string_view::npos ? std::string_view{} : rest.substr(plus + 1);
    }
    return mask;
}

std::optional<InputScript> parseInputScript(std::string_view text, std::string& error) {
    InputScript script;
    std::size_t lineNo = 0;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        ++lineNo;
        line = line.substr(0, line.find('#'));
        const auto t = tokens(line);
        if (t.empty()) {
            continue;
        }
        const auto fail = [&](const std::string& what) {
            error = "line " + std::to_string(lineNo) + ": " + what;
            return std::nullopt;
        };
        if (t[0] == "rom") {
            if (t.size() != 2 || t[1].size() != 40 ||
                !std::all_of(t[1].begin(), t[1].end(), [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)) != 0; })) {
                return fail("expected 'rom <40-digit SHA-1>'");
            }
            script.romSha1.assign(t[1]);
            std::transform(script.romSha1.begin(), script.romSha1.end(), script.romSha1.begin(),
                           [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });
            continue;
        }
        if (t[0] == "checkpoint") {
            ScriptCheckpoint c;
            c.line = lineNo;
            if (t.size() < 3 || t.size() > 4 || !parseU64(t[1], c.frame)) {
                return fail("expected 'checkpoint <frame> frame_hash|state_hash|audio_hash [<hash>]'");
            }
            if (t[2] == "frame_hash") {
                c.kind = CheckKind::FrameHash;
            } else if (t[2] == "state_hash") {
                c.kind = CheckKind::StateHash;
            } else if (t[2] == "audio_hash") {
                c.kind = CheckKind::AudioHash;
            } else {
                return fail("unknown checkpoint kind '" + std::string(t[2]) + "'");
            }
            if (t.size() == 4) {
                if (!isHash(t[3])) {
                    return fail("expected a 16-digit hex hash");
                }
                std::string h(t[3]);
                std::transform(h.begin(), h.end(), h.begin(),
                               [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });
                c.expected = h;
            }
            script.checkpoints.push_back(c);
            continue;
        }
        ScriptEvent e;
        std::uint64_t port = 0;
        if (t.size() != 3 || !parseU64(t[0], e.frame) || !parseU64(t[1], port) || port < 1 || port > 2) {
            return fail("expected '<frame> <port 1|2> <buttons>'");
        }
        const auto buttons = parseButtons(t[2]);
        if (!buttons) {
            return fail("unknown button in '" + std::string(t[2]) + "'");
        }
        e.port = static_cast<int>(port - 1);
        e.buttons = *buttons;
        script.events.push_back(e);
    }
    std::stable_sort(script.events.begin(), script.events.end(),
                     [](const ScriptEvent& a, const ScriptEvent& b) { return a.frame < b.frame; });
    return script;
}

std::string updateScriptHashes(std::string_view text, const std::map<std::size_t, std::string>& hashes) {
    std::string out;
    std::size_t lineNo = 0;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string line(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        ++lineNo;
        if (const auto it = hashes.find(lineNo); it != hashes.end()) {
            const std::size_t hash = line.find('#');
            std::string comment = hash == std::string::npos ? "" : line.substr(hash);
            auto t = tokens(std::string_view(line).substr(0, hash));
            std::string rebuilt;
            for (std::size_t i = 0; i < 3 && i < t.size(); ++i) {
                rebuilt += (i != 0 ? " " : "") + std::string(t[i]);
            }
            rebuilt += " " + it->second;
            if (!comment.empty()) {
                rebuilt += "  " + comment;
            }
            line = rebuilt;
        }
        out += line;
        if (nl != std::string_view::npos) {
            out += '\n';
        }
    }
    return out;
}

void InputPlayer::beforeFrame(Machine& m) {
    // Input for frame F must be pending before the F-1 -> F boundary, i.e. while frame F-1 runs.
    while (next_ < script_.events.size() && script_.events[next_].frame <= m.frame() + 1) {
        const ScriptEvent& e = script_.events[next_++];
        m.setInput(e.port, e.buttons);
    }
}

}  // namespace gen::common
