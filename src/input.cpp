#include "input.h"

#include <poll.h>
#include <unistd.h>

namespace {

// Escape sequences for special keys look like ESC '[' <parameters> <final>,
// for example ESC [ A or, with Ctrl held, ESC [ 1 ; 5 A. The parameters are
// digits and ';', and the final byte, which says what the sequence means, is
// always in the range '@' to '~'. Some terminals send arrows as ESC 'O' <final>.
bool isFinalByte(char c) {
    return c >= 0x40 && c <= 0x7e;
}

// Decodes the body of an SGR mouse report: "0;12;5" from ESC [ < 0;12;5 M.
// The three numbers are a button code, then the column and row (counting from
// 1). The final byte is 'M' for a press or movement and 'm' for a release.
// Returns false if the report is malformed or isn't one we use.
bool parseMouse(const std::string& params, char final, InputEvent& event) {
    using Type = InputEvent::Type;

    // Split "b;x;y" into three numbers.
    int values[3] = {};
    int count = 0;
    bool haveDigit = false;
    for (char c : params) {
        if (c >= '0' && c <= '9') {
            if (values[count] > 100000) return false; // nonsense; stop before it overflows
            values[count] = values[count] * 10 + (c - '0');
            haveDigit = true;
        } else if (c == ';' && haveDigit && count < 2) {
            ++count;
            haveDigit = false;
        } else {
            return false;
        }
    }
    if (count != 2 || !haveDigit) return false;

    event.x = values[1] - 1;
    event.y = values[2] - 1;

    // The button code packs several things into its bits:
    //   bits 0-1 (1, 2):    which button: 0 left, 1 middle, 2 right
    //   bits 2-4 (4-16):    Shift, Alt, Ctrl held; we ignore those
    //   bit 5 (32):         the mouse moved while a button was held
    //   bit 6 (64):         the scroll wheel: 64 is up, 65 is down
    const int code = values[0] & ~(4 | 8 | 16);

    if (code & 64) {
        if (final != 'M') return false;
        if ((code & 3) == 0) event.type = Type::ScrollUp;
        else if ((code & 3) == 1) event.type = Type::ScrollDown;
        else return false; // sideways scrolling
    } else if (final == 'm') {
        event.type = Type::MouseRelease;
    } else if (code & 32) {
        event.type = Type::MouseDrag;
    } else {
        event.type = Type::MousePress;
    }
    return true;
}

} // namespace

std::vector<InputEvent> parseInput(const std::string& bytes, std::size_t& consumed) {
    using Type = InputEvent::Type;
    std::vector<InputEvent> events;

    std::size_t i = 0;
    while (i < bytes.size()) {
        const char c = bytes[i];

        if (c != '\x1b') {
            events.push_back({Type::Char, c});
            ++i;
            continue;
        }

        // ESC with nothing after it: the Esc key itself. The terminal sends a
        // whole escape sequence in one go, so a lone ESC at the end of what we
        // read is almost certainly the key.
        if (i + 1 == bytes.size() || (bytes[i + 1] != '[' && bytes[i + 1] != 'O')) {
            events.push_back({Type::Escape});
            ++i;
            continue;
        }

        // Find the final byte of the sequence.
        std::size_t end = i + 2;
        while (end < bytes.size() && !isFinalByte(bytes[end])) ++end;
        if (end == bytes.size()) break; // incomplete; wait for the rest

        // Mouse reports start with ESC [ < and end with M or m.
        const bool isMouse = bytes[i + 1] == '[' && i + 2 < end && bytes[i + 2] == '<' &&
                             (bytes[end] == 'M' || bytes[end] == 'm');
        if (isMouse) {
            InputEvent event{Type::MousePress};
            if (parseMouse(bytes.substr(i + 3, end - (i + 3)), bytes[end], event)) {
                events.push_back(event);
            }
            i = end + 1;
            continue;
        }

        switch (bytes[end]) {
            case 'A': events.push_back({Type::Up}); break;
            case 'B': events.push_back({Type::Down}); break;
            case 'C': events.push_back({Type::Right}); break;
            case 'D': events.push_back({Type::Left}); break;
            default: break; // a key we don't use (F1, Home, ...): skip it
        }
        i = end + 1;
    }

    consumed = i;
    return events;
}

std::vector<InputEvent> InputReader::readEvents() {
    // poll() with a timeout of 0 only checks whether input is waiting; it
    // never sleeps. Read until nothing is left.
    pollfd pfd{STDIN_FILENO, POLLIN, 0};
    char buf[256];
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        const ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) break; // end of input (e.g. stdin is /dev/null) or an error
        pending_.append(buf, static_cast<std::size_t>(n));
    }

    std::size_t consumed = 0;
    std::vector<InputEvent> events = parseInput(pending_, consumed);
    pending_.erase(0, consumed);
    return events;
}
