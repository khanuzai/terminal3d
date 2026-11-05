#include "terminal.h"

#include <cerrno>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

// ANSI escape sequences. Each one starts with ESC (byte 0x1b) followed by '['.
// The terminal treats them as commands instead of printing them.
constexpr const char* kEnterAltScreen = "\x1b[?1049h";
constexpr const char* kLeaveAltScreen = "\x1b[?1049l";
constexpr const char* kHideCursor     = "\x1b[?25l";
constexpr const char* kShowCursor     = "\x1b[?25h";
constexpr const char* kClearScreen    = "\x1b[2J";

} // namespace

TermSize terminalSize() {
    // TIOCGWINSZ ("terminal I/O control: get window size") asks the terminal
    // driver for the current size, in rows and columns.
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return {ws.ws_col, ws.ws_row};
    }
    return {80, 24};
}

void writeToTerminal(const std::string& data) {
    const char* p = data.data();
    std::size_t left = data.size();

    // write() may accept fewer bytes than requested, so loop until everything
    // is written. EINTR means a signal interrupted the call; just try again.
    while (left > 0) {
        const ssize_t n = write(STDOUT_FILENO, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return; // the terminal went away; nothing useful left to do
        }
        p += n;
        left -= static_cast<std::size_t>(n);
    }
}

TerminalSession::TerminalSession() {
    writeToTerminal(std::string(kEnterAltScreen) + kHideCursor + kClearScreen);
}

TerminalSession::~TerminalSession() {
    writeToTerminal(std::string(kShowCursor) + kLeaveAltScreen);
}
