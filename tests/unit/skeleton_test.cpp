#include <catch2/catch_test_macros.hpp>

#include "analysis/analysis.hpp"
#include "core/version.hpp"
#include "instrument/instrument.hpp"

TEST_CASE("Libraries link together", "[skeleton]") {
    CHECK_FALSE(gen::version().empty());
    CHECK(gen::instrument::available());
    CHECK(gen::analysis::available());
}
