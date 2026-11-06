#include "common/config.hpp"

#include <array>
#include <fstream>
#include <system_error>

#include "common/paths.hpp"

namespace gen::common {

using nlohmann::json;

namespace {

template <typename T>
bool holds(const json& value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value.is_boolean();
    } else if constexpr (std::is_integral_v<T>) {
        return value.is_number_integer();
    } else if constexpr (std::is_same_v<T, std::string>) {
        return value.is_string();
    } else {
        return false;
    }
}

class Reader {
public:
    explicit Reader(std::vector<std::string>* warnings) : warnings_(warnings) {}

    template <typename T>
    void read(const json& obj, const char* key, T& out, const std::string& where) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return;
        }
        if (!holds<T>(*it)) {
            warn("'" + where + key + "' has the wrong type; using the default");
            return;
        }
        out = it->template get<T>();
    }

    const json* object(const json& obj, const char* key, const std::string& where) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return nullptr;
        }
        if (!it->is_object()) {
            warn("'" + where + key + "' is not an object; using defaults");
            return nullptr;
        }
        return &*it;
    }

    void warn(std::string message) {
        if (warnings_ != nullptr) {
            warnings_->push_back(std::move(message));
        }
    }

private:
    std::vector<std::string>* warnings_;
};

int schemaVersionOf(const json& doc) {
    const auto it = doc.find("schemaVersion");
    return it != doc.end() && it->is_number_integer() ? it->get<int>() : 0;
}

bool writeFile(const std::filesystem::path& path, const json& doc, std::string& error) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "Cannot create '" + toUtf8(path.parent_path()) + "': " + ec.message();
            return false;
        }
    }
    // Write to a temporary file first so a crash never leaves a truncated config behind.
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Cannot write '" + toUtf8(tmp) + "'";
            return false;
        }
        out << doc.dump(2) << '\n';
        if (!out) {
            error = "Cannot write '" + toUtf8(tmp) + "'";
            return false;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        error = "Cannot replace '" + toUtf8(path) + "': " + ec.message();
        return false;
    }
    return true;
}

}  // namespace

void migrateConfig(json& doc) {
    int version = schemaVersionOf(doc);
    if (version > kConfigSchemaVersion) {
        return;
    }
    // kMigrations[N] upgrades a version-N document to version N+1.
    using Migration = void (*)(json&);
    static constexpr std::array<Migration, kConfigSchemaVersion> kMigrations{
        [](json&) {},  // 0 -> 1: unversioned files already have the version-1 shape
    };
    for (; version < kConfigSchemaVersion; ++version) {
        kMigrations[static_cast<std::size_t>(version)](doc);
    }
    doc["schemaVersion"] = version;
}

InputConfig defaultInputConfig() {
    InputConfig c;
    c.keys[0] = {{"UP", "Up"}, {"DOWN", "Down"}, {"LEFT", "Left"},    {"RIGHT", "Right"},
                 {"A", "A"},   {"B", "S"},       {"C", "D"},          {"X", "Q"},
                 {"Y", "W"},   {"Z", "E"},       {"START", "Return"}, {"MODE", "Right Shift"}};
    c.gamepad = {{"UP", "dpup"},     {"DOWN", "dpdown"},
                 {"LEFT", "dpleft"}, {"RIGHT", "dpright"},
                 {"A", "x"},         {"B", "a"},
                 {"C", "b"},         {"X", "leftshoulder"},
                 {"Y", "y"},         {"Z", "rightshoulder"},
                 {"START", "start"}, {"MODE", "back"}};
    c.hotkeys = {{"pause", "F5"},
                 {"step-instruction", "F11"},
                 {"step-over", "F10"},
                 {"step-out", "Shift+F11"},
                 {"step-frame", "F6"},
                 {"step-scanline", "Shift+F6"},
                 {"fast-forward", "Tab"},
                 {"save-state-1", "Shift+F1"},
                 {"save-state-2", "Shift+F2"},
                 {"save-state-3", "Shift+F3"},
                 {"save-state-4", "Shift+F4"},
                 {"load-state-1", "F1"},
                 {"load-state-2", "F2"},
                 {"load-state-3", "F3"},
                 {"load-state-4", "F4"}};
    return c;
}

namespace {

void readStringMap(const json& obj, std::map<std::string, std::string>& out) {
    for (const auto& [k, v] : obj.items()) {
        if (v.is_string()) {
            out[k] = v.get<std::string>();
        }
    }
}

}  // namespace

Config configFromJson(const json& doc, std::vector<std::string>* warnings) {
    Config c;
    if (!doc.is_object()) {
        if (warnings != nullptr) {
            warnings->emplace_back("config root is not an object; using defaults");
        }
        return c;
    }
    Reader r(warnings);

    if (const json* log = r.object(doc, "log", "")) {
        r.read(*log, "level", c.log.level, "log.");
        r.read(*log, "toFile", c.log.toFile, "log.");
        r.read(*log, "maxFileSizeKb", c.log.maxFileSizeKb, "log.");
        r.read(*log, "maxFiles", c.log.maxFiles, "log.");
        if (const json* subs = r.object(*log, "subsystems", "log.")) {
            for (const auto& [name, level] : subs->items()) {
                if (level.is_string()) {
                    c.log.subsystems[name] = level.get<std::string>();
                } else {
                    r.warn("'log.subsystems." + name + "' is not a string; ignored");
                }
            }
        }
    }

    if (const json* ui = r.object(doc, "ui", "")) {
        r.read(*ui, "windowWidth", c.ui.windowWidth, "ui.");
        r.read(*ui, "windowHeight", c.ui.windowHeight, "ui.");
        r.read(*ui, "windowMaximized", c.ui.windowMaximized, "ui.");
        r.read(*ui, "dockLayout", c.ui.dockLayout, "ui.");
        r.read(*ui, "maxRecentRoms", c.ui.maxRecentRoms, "ui.");
        if (const auto it = ui->find("recentRoms"); it != ui->end()) {
            if (it->is_array()) {
                for (const json& entry : *it) {
                    if (entry.is_string()) {
                        c.ui.recentRoms.push_back(entry.get<std::string>());
                    }
                }
            } else {
                r.warn("'ui.recentRoms' is not an array; ignored");
            }
        }
    }
    if (const json* view = r.object(doc, "view", "")) {
        r.read(*view, "scale", c.view.scale, "view.");
        r.read(*view, "aspectCorrect", c.view.aspectCorrect, "view.");
        r.read(*view, "linearFilter", c.view.linearFilter, "view.");
        r.read(*view, "showOverlay", c.view.showOverlay, "view.");
    }
    if (const json* input = r.object(doc, "input", "")) {
        for (std::size_t p = 0; p < 2; ++p) {
            const std::string port = "port" + std::to_string(p + 1);
            if (const json* pj = r.object(*input, port.c_str(), "input.")) {
                r.read(*pj, "padType", c.input.padType[p], "input." + port + ".");
                if (const json* keys = r.object(*pj, "keys", "input." + port + ".")) {
                    c.input.keys[p].clear();
                    readStringMap(*keys, c.input.keys[p]);
                }
            }
        }
        if (const json* pad = r.object(*input, "gamepad", "input.")) {
            c.input.gamepad.clear();
            readStringMap(*pad, c.input.gamepad);
        }
        if (const json* hot = r.object(*input, "hotkeys", "input.")) {
            readStringMap(*hot, c.input.hotkeys);  // over the defaults: new actions keep their default key
        }
    }
    return c;
}

void configToJson(const Config& c, json& doc) {
    if (!doc.is_object()) {
        doc = json::object();
    }
    if (schemaVersionOf(doc) < kConfigSchemaVersion) {
        doc["schemaVersion"] = kConfigSchemaVersion;
    }

    json& log = doc["log"];
    if (!log.is_object()) {
        log = json::object();
    }
    log["level"] = c.log.level;
    log["subsystems"] = c.log.subsystems;
    log["toFile"] = c.log.toFile;
    log["maxFileSizeKb"] = c.log.maxFileSizeKb;
    log["maxFiles"] = c.log.maxFiles;

    json& ui = doc["ui"];
    if (!ui.is_object()) {
        ui = json::object();
    }
    ui["windowWidth"] = c.ui.windowWidth;
    ui["windowHeight"] = c.ui.windowHeight;
    ui["windowMaximized"] = c.ui.windowMaximized;
    ui["dockLayout"] = c.ui.dockLayout;
    ui["recentRoms"] = c.ui.recentRoms;
    ui["maxRecentRoms"] = c.ui.maxRecentRoms;

    json& view = doc["view"];
    if (!view.is_object()) {
        view = json::object();
    }
    view["scale"] = c.view.scale;
    view["aspectCorrect"] = c.view.aspectCorrect;
    view["linearFilter"] = c.view.linearFilter;
    view["showOverlay"] = c.view.showOverlay;

    json& input = doc["input"];
    if (!input.is_object()) {
        input = json::object();
    }
    for (std::size_t p = 0; p < 2; ++p) {
        json& pj = input["port" + std::to_string(p + 1)];
        if (!pj.is_object()) {
            pj = json::object();
        }
        pj["padType"] = c.input.padType[p];
        pj["keys"] = c.input.keys[p];
    }
    input["gamepad"] = c.input.gamepad;
    input["hotkeys"] = c.input.hotkeys;
}

ConfigFile ConfigFile::loadOrCreate(const std::filesystem::path& path) {
    ConfigFile file;
    file.path_ = path;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        file.save();
        return file;
    }

    std::ifstream in(path, std::ios::binary);
    json doc = json::parse(in, nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
    if (!in.is_open() || doc.is_discarded() || !doc.is_object()) {
        in.close();
        auto bad = path;
        bad += ".bad";
        std::filesystem::rename(path, bad, ec);
        file.warnings.push_back("'" + toUtf8(path) + "' is not a valid config file" +
                                (ec ? std::string() : "; moved to '" + toUtf8(bad) + "'") +
                                "; using defaults");
        file.save();
        return file;
    }

    if (schemaVersionOf(doc) > kConfigSchemaVersion) {
        file.warnings.push_back("config schema version " + std::to_string(schemaVersionOf(doc)) +
                                " is newer than this build supports (" +
                                std::to_string(kConfigSchemaVersion) + "); unknown settings are preserved");
    }
    migrateConfig(doc);
    file.config = configFromJson(doc, &file.warnings);
    file.doc_ = std::move(doc);
    return file;
}

bool ConfigFile::save() {
    configToJson(config, doc_);
    std::string error;
    if (!writeFile(path_, doc_, error)) {
        warnings.push_back(std::move(error));
        return false;
    }
    return true;
}

}  // namespace gen::common
