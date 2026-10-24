// Writes synthetic ROM files for the gen_cli tests: synthetic.bin, synthetic.smd (same image) and tiny.bin.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>

#include "support/synthetic_rom.hpp"

namespace {

bool write(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: make_test_roms <output-dir>\n");
        return 1;
    }
    const std::filesystem::path dir = argv[1];
    std::filesystem::create_directories(dir);

    gen::test::SyntheticRomOptions options;
    options.domesticTitle = "SYNTHETIC CLI ROM";
    options.overseasTitle = "SYNTHETIC CLI ROM";
    const auto image = gen::test::makeSyntheticRom(options);

    gen::test::SyntheticRomOptions tiny;
    tiny.size = 100;

    const bool ok = write(dir / "synthetic.bin", image) &&
                    write(dir / "synthetic.smd", gen::test::encodeSmd(image)) &&
                    write(dir / "tiny.bin", gen::test::makeSyntheticRom(tiny));
    return ok ? 0 : 1;
}
