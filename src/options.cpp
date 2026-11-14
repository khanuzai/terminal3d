#include "options.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

// Formats a number for an error message: 10 rather than 10.000000.
std::string str(float v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

// Parses a whole string as an integer in [lo, hi]. Rejects things like "",
// "12abc" and "1e3", which std::stoi would quietly accept or partly accept.
int parseInt(const std::string& option, const std::string& text, int lo, int hi) {
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || *end != '\0' || errno == ERANGE || v < lo || v > hi) {
        throw std::invalid_argument(option + " expects a whole number from " + std::to_string(lo) +
                                    " to " + std::to_string(hi) + ", got '" + text + "'");
    }
    return static_cast<int>(v);
}

// Parses a whole string as a number in [lo, hi].
float parseFloat(const std::string& option, const std::string& text, float lo, float hi) {
    char* end = nullptr;
    errno = 0;
    const float v = std::strtof(text.c_str(), &end);
    if (text.empty() || *end != '\0' || errno == ERANGE || !std::isfinite(v) || v < lo || v > hi) {
        throw std::invalid_argument(option + " expects a number from " + str(lo) + " to " +
                                    str(hi) + ", got '" + text + "'");
    }
    return v;
}

// Parses a hex color like "ff963c" or "#ff963c".
Color parseColor(const std::string& option, const std::string& text) {
    const std::string hex = !text.empty() && text[0] == '#' ? text.substr(1) : text;

    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    int channels[3] = {};
    bool ok = hex.size() == 6;
    for (int i = 0; ok && i < 3; ++i) {
        const int high = digit(hex[2 * i]);
        const int low = digit(hex[2 * i + 1]);
        ok = high >= 0 && low >= 0;
        channels[i] = high * 16 + low;
    }
    if (!ok) {
        throw std::invalid_argument(option + " expects a hex color like ff963c, got '" + text + "'");
    }
    return {static_cast<std::uint8_t>(channels[0]), static_cast<std::uint8_t>(channels[1]),
            static_cast<std::uint8_t>(channels[2])};
}

} // namespace

Options parseOptions(int argc, const char* const* argv) {
    Options opts;
    std::vector<std::string> positional;
    bool optionsEnded = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        // Anything that doesn't start with '-' is a file name. So is "-" on
        // its own, and everything after "--".
        if (optionsEnded || arg.size() < 2 || arg[0] != '-') {
            positional.push_back(arg);
            continue;
        }
        if (arg == "--") {
            optionsEnded = true;
            continue;
        }

        // Split "--name=value" into its name and value.
        std::string name = arg;
        std::optional<std::string> attachedValue;
        const std::size_t eq = arg.find('=');
        if (arg.compare(0, 2, "--") == 0 && eq != std::string::npos) {
            name = arg.substr(0, eq);
            attachedValue = arg.substr(eq + 1);
        }

        // The option's value: the part after '=', or else the next argument.
        // Taking the next argument as-is lets values start with '-' (--spin -1).
        auto value = [&]() -> std::string {
            if (attachedValue) return *attachedValue;
            if (i + 1 >= argc) throw std::invalid_argument(name + " needs a value");
            return argv[++i];
        };
        // For flags: reject "--ascii=yes", which probably isn't what was meant.
        auto noValue = [&]() {
            if (attachedValue) throw std::invalid_argument(name + " doesn't take a value");
        };

        if (name == "-h" || name == "--help") { noValue(); opts.showHelp = true; }
        else if (name == "--ascii")           { noValue(); opts.mode = OutputMode::Ascii; }
        else if (name == "--light")           { noValue(); opts.lightBackground = true; }
        else if (name == "--wireframe")       { noValue(); opts.wireframe = true; }
        else if (name == "--color")           opts.color = parseColor(name, value());
        else if (name == "--fps")             opts.fps = parseInt(name, value(), 1, 120);
        else if (name == "--spin")            opts.spinSpeed = parseFloat(name, value(), -10.0f, 10.0f);
        else throw std::invalid_argument("unknown option '" + arg + "'");
    }

    if (opts.showHelp) return opts;

    if (positional.empty()) throw std::invalid_argument("no model file given");
    if (positional.size() > 1) {
        throw std::invalid_argument("expected one model file, got " +
                                    std::to_string(positional.size()) + " ('" + positional[0] +
                                    "', '" + positional[1] + "'" +
                                    (positional.size() > 2 ? ", ...)" : ")"));
    }
    opts.modelPath = positional[0];
    return opts;
}

std::string usageText() {
    return
        "usage: term3d [options] <model.obj>\n"
        "\n"
        "Shows a 3D model, spinning and lit, right in the terminal.\n"
        "\n"
        "options:\n"
        "  --ascii          draw with ASCII characters instead of colored half-blocks\n"
        "  --light          for light-background terminals: reverse the ASCII ramp\n"
        "  --color RRGGBB   model color in hex (default ff963c)\n"
        "  --fps N          target frame rate, 1 to 120 (default 30)\n"
        "  --spin SPEED     radians per second, -10 to 10; 0 = still (default 0.8)\n"
        "  --wireframe      start in wireframe mode\n"
        "  -h, --help       show this help and exit\n"
        "\n"
        "controls:\n"
        "  drag, arrow keys   rotate          scroll, + -   zoom\n"
        "  space              pause the spin  w             wireframe on/off\n"
        "  r                  reset the view  q, Esc        quit\n";
}
