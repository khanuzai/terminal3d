#include <stdexcept>
#include <vector>

#include "options.h"
#include "test.h"

namespace {

// Parses arguments as if typed after "term3d" on the command line.
Options parse(std::vector<const char*> args) {
    args.insert(args.begin(), "term3d");
    return parseOptions(static_cast<int>(args.size()), args.data());
}

} // namespace

TEST(options_defaults) {
    const Options o = parse({"model.obj"});
    CHECK_EQ(o.modelPath, std::string("model.obj"));
    CHECK_EQ(o.mode, OutputMode::HalfBlock);
    CHECK_EQ(o.fps, 30);
    CHECK_EQ(o.faces, FaceMode::Auto);
    CHECK(o.smooth);
    CHECK(!o.showHelp);
}

TEST(options_flags_and_both_value_styles) {
    const Options o = parse({"--ascii", "--fps", "60", "model.obj", "--spin=-1.5",
                             "--color", "#00ff80", "--flat", "--two-sided", "--crease=30"});
    CHECK_EQ(o.mode, OutputMode::Ascii);
    CHECK_EQ(o.fps, 60);
    CHECK_NEAR(o.spinSpeed, -1.5, 1e-6); // a value may start with '-'
    CHECK(o.color == (Color{0, 255, 128}));
    CHECK(!o.smooth);
    CHECK_EQ(o.faces, FaceMode::TwoSided);
    CHECK_NEAR(o.creaseAngle, 30.0, 1e-6);
    CHECK_EQ(o.modelPath, std::string("model.obj"));
}

TEST(options_range_edges_are_allowed) {
    CHECK_EQ(parse({"--fps", "1", "a.obj"}).fps, 1);
    CHECK_EQ(parse({"--fps", "120", "a.obj"}).fps, 120);
    CHECK_NEAR(parse({"--crease", "180", "a.obj"}).creaseAngle, 180.0, 1e-6);
}

TEST(options_double_dash_ends_options) {
    const Options o = parse({"--", "--weird-name.obj"});
    CHECK_EQ(o.modelPath, std::string("--weird-name.obj"));
}

TEST(options_help_needs_no_model) {
    CHECK(parse({"--help"}).showHelp);
    CHECK(parse({"-h"}).showHelp);
}

TEST(options_errors_explain_the_problem) {
    using E = std::invalid_argument;
    CHECK_THROWS(E, parse({}), "no model file given");
    CHECK_THROWS(E, parse({"a.obj", "b.obj"}), "expected one model file, got 2 ('a.obj', 'b.obj')");
    CHECK_THROWS(E, parse({"--bogus", "a.obj"}), "unknown option '--bogus'");
    CHECK_THROWS(E, parse({"a.obj", "--fps"}), "--fps needs a value");
    // Ranges: test right at both edges, where off-by-one mistakes hide.
    CHECK_THROWS(E, parse({"--fps", "0", "a.obj"}), "from 1 to 120, got '0'");
    CHECK_THROWS(E, parse({"--fps", "121", "a.obj"}), "from 1 to 120, got '121'");
    CHECK_THROWS(E, parse({"--crease", "180.5", "a.obj"}), "from 0 to 180");
    CHECK_THROWS(E, parse({"--fps", "30fps", "a.obj"}), "got '30fps'");
    CHECK_THROWS(E, parse({"--spin", "nan", "a.obj"}), "got 'nan'");
    CHECK_THROWS(E, parse({"--color", "orange", "a.obj"}), "hex color");
    CHECK_THROWS(E, parse({"--ascii=yes", "a.obj"}), "--ascii doesn't take a value");
}
