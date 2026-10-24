#include <cstdio>

#include "core/version.hpp"

int main() {
    std::printf("gen_cli %.*s\n", static_cast<int>(gen::version().size()), gen::version().data());
    return 0;
}
