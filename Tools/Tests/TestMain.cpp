#include "Tools/Tests/Check.h"

int main() {
    for (const auto& test : Check::Cases()) {
        int before = Check::Failures();
        test.run();
        std::printf("%s %s\n", Check::Failures() == before ? "ok  " : "FAIL", test.name);
    }

    std::printf("%zu tests, %d failed checks\n", Check::Cases().size(), Check::Failures());
    return Check::Failures() ? 1 : 0;
}
