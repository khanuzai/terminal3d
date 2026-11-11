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
