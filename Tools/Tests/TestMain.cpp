#include <cstring>
#include "Tools/Tests/Check.h"

// Optional arguments: run only the cases whose name contains one of them.
static bool Selected(const char* name, int argc, char** argv) {
    if (argc < 2) {
        return true;
    }
    for (int i = 1; i < argc; i++) {
        if (std::strstr(name, argv[i])) {
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    size_t run = 0;
    for (const auto& test : Check::Cases()) {
        if (!Selected(test.name, argc, argv)) {
            continue;
        }
        int before = Check::Failures();
        test.run();
        run++;
        std::printf("%s %s\n", Check::Failures() == before ? "ok  " : "FAIL", test.name);
    }

    std::printf("%zu tests, %d failed checks\n", run, Check::Failures());
    return Check::Failures() ? 1 : 0;
}
