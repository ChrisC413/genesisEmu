#pragma once
// Scripted input for tests (spec 13 "Scripted input format", M04-03). Not a user-facing movie format (D-007).
//
//   # frame  port  buttons            buttons: '-' or names joined by '+':
//   0        1     -                  UP DOWN LEFT RIGHT A B C START X Y Z MODE
//   120      1     START
//   300      1     RIGHT+C
//   checkpoint 600 frame_hash 1a2b3c4d5e6f7a8b   # the expected hash is optional
//   checkpoint 600 state_hash
//   rom 69e102855d4389c3fd1a8f3dc7d193f8eee5fe5b   # optional: the image (SHA-1) the script is for
//
// An input line holds its buttons from that frame until the next line for the same port; it is latched at the
// start of the frame (frame 0 = from the first frame boundary on). A checkpoint at frame N is evaluated after
// N frames have run. Text after '#' is a comment.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gen {
class Machine;
}

namespace gen::common {

enum class CheckKind : std::uint8_t { FrameHash, StateHash, AudioHash };
const char* toString(CheckKind kind);

struct ScriptEvent {
    std::uint64_t frame = 0;
    int port = 0;  // 0-based
    std::uint16_t buttons = 0;
};

struct ScriptCheckpoint {
    std::uint64_t frame = 0;
    CheckKind kind = CheckKind::FrameHash;
    std::optional<std::string> expected;  // lowercase hex
    std::size_t line = 0;                 // 1-based source line
};

struct InputScript {
    std::vector<ScriptEvent> events;  // sorted by frame (stable)
    std::vector<ScriptCheckpoint> checkpoints;
    std::string romSha1;              // lowercase hex; empty when the script does not name its ROM
    std::uint64_t lastFrame() const;  // the latest frame any line names
};

// Returns nullopt and sets `error` ("line N: ...") on a syntax error.
std::optional<InputScript> parseInputScript(std::string_view text, std::string& error);

// Parses "UP+RIGHT+C" or "-" into a bus::PadButton mask.
std::optional<std::uint16_t> parseButtons(std::string_view text);

// Replaces (or appends) the expected hash of the checkpoint lines in `hashes` (1-based line -> hex),
// keeping comments. Used to record goldens.
std::string updateScriptHashes(std::string_view text, const std::map<std::size_t, std::string>& hashes);

// Feeds the script's input into a Machine frame by frame. Call before each run of one frame.
class InputPlayer {
public:
    explicit InputPlayer(const InputScript& script) : script_(script) {}
    void beforeFrame(Machine& m);

private:
    const InputScript& script_;
    std::size_t next_ = 0;
};

}  // namespace gen::common
