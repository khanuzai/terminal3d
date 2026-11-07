#include "terminal.h"

#include <cerrno>
#include <csignal>
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
constexpr const char* kBeginSync      = "\x1b[?2026h";
constexpr const char* kEndSync        = "\x1b[?2026l";

// Flags set by the signal handlers. A signal can arrive between any two
// instructions of the program, so a handler may only do a tiny set of
// "async-signal-safe" things. Writing a volatile sig_atomic_t is one of them.
volatile std::sig_atomic_t gQuitRequested = 0;
volatile std::sig_atomic_t gResized = 0;

void onQuitSignal(int) { gQuitRequested = 1; }
void onResizeSignal(int) { gResized = 1; }

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

void presentFrame(const std::string& frame) {
    writeToTerminal(kBeginSync + frame + kEndSync);
}

void clearTerminal() {
    writeToTerminal(kClearScreen);
}

void installSignalHandlers() {
    struct sigaction sa {};
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = onQuitSignal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    sa.sa_handler = onResizeSignal;
    sigaction(SIGWINCH, &sa, nullptr);
}

bool quitRequested() {
    return gQuitRequested != 0;
}

bool consumeResize() {
    if (!gResized) return false;
    // Clear the flag *before* the caller reads the new size: if another resize
    // arrives right after this, the flag gets set again and we catch it next frame.
    gResized = 0;
    return true;
}

TerminalSession::TerminalSession() {
    writeToTerminal(std::string(kEnterAltScreen) + kHideCursor + kClearScreen);
}

TerminalSession::~TerminalSession() {
    writeToTerminal(std::string(kShowCursor) + kLeaveAltScreen);
}
