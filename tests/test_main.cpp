// Runs every registered test and reports the results.
//
//   term3d_tests            run everything
//   term3d_tests options    run only tests whose name contains "options"
//
// Exits with 0 if everything passed and 1 otherwise, which is what CTest and
// CI look at to decide pass or fail.

#include <exception>
#include <iostream>
#include <string>

#include "test.h"

int main(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";

    int run = 0;
    int failed = 0;
    for (const test::TestCase& t : test::registry()) {
        if (std::string(t.name).find(filter) == std::string::npos) continue;

        ++run;
        test::currentFailures() = 0;
        try {
            t.run();
        } catch (const test::AbortTest&) {
            // A REQUIRE failed; it already reported why.
        } catch (const std::exception& e) {
            test::fail("(test)", 0, std::string("unexpected exception: ") + e.what());
        } catch (...) {
            test::fail("(test)", 0, "unexpected exception of unknown type");
        }

        const bool passed = test::currentFailures() == 0;
        if (!passed) ++failed;
        std::cout << (passed ? "pass  " : "FAIL  ") << t.name << "\n";
    }

    std::cout << "\n" << run - failed << " of " << run << " tests passed\n";
    if (run == 0) {
        std::cout << "no tests match '" << filter << "'\n";
        return 1;
    }
    return failed == 0 ? 0 : 1;
}
