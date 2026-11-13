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

// Mouse reporting. Mode 1002 reports button presses, releases, and movement
// while a button is held (enough for dragging, without a flood of events
// every time the mouse merely passes over the window). Mode 1006 picks the
// "SGR" format for those reports, which handles windows of any size.
constexpr const char* kEnableMouse    = "\x1b[?1002h\x1b[?1006h";
constexpr const char* kDisableMouse   = "\x1b[?1006l\x1b[?1002l";

// Flags set by the signal handlers. A signal can arrive between any two
// instructions of the program, so a handler may only do a tiny set of
// "async-signal-safe" things. Writing a volatile sig_atomic_t is one of them.
volatile std::sig_atomic_t gQuitRequested = 0;
volatile std::sig_atomic_t gResized = 0;
volatile std::sig_atomic_t gSuspendRequested = 0;
volatile std::sig_atomic_t gContinued = 0;

void onQuitSignal(int) { gQuitRequested = 1; }
void onResizeSignal(int) { gResized = 1; }
void onSuspendSignal(int) { gSuspendRequested = 1; }
void onContinueSignal(int) { gContinued = 1; }

void setHandler(int signal, void (*handler)(int)) {
    struct sigaction sa {};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handler;
    sigaction(signal, &sa, nullptr);
}

// Shared by the consume* functions. Clears the flag *before* the caller
// reacts: if the signal arrives again meanwhile, the flag gets set again and
// we catch it next frame instead of losing it.
bool consumeFlag(volatile std::sig_atomic_t& flag) {
    if (!flag) return false;
    flag = 0;
    return true;
}

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

bool writeToTerminal(const std::string& data) {
    const char* p = data.data();
    std::size_t left = data.size();

    // write() may accept fewer bytes than requested, so loop until everything
    // is written. EINTR means a signal interrupted the call; just try again.
    while (left > 0) {
        const ssize_t n = write(STDOUT_FILENO, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false; // e.g. EIO: the terminal was closed
        }
        p += n;
        left -= static_cast<std::size_t>(n);
    }
    return true;
}

bool presentFrame(const std::string& frame) {
    return writeToTerminal(kBeginSync + frame + kEndSync);
}

void clearTerminal() {
    writeToTerminal(kClearScreen);
}

void installSignalHandlers() {
    setHandler(SIGINT, onQuitSignal);
    setHandler(SIGTERM, onQuitSignal);
    setHandler(SIGHUP, onQuitSignal);
    setHandler(SIGWINCH, onResizeSignal);
    setHandler(SIGTSTP, onSuspendSignal);
    setHandler(SIGCONT, onContinueSignal);
}

bool quitRequested() {
    return gQuitRequested != 0;
}

bool consumeResize() { return consumeFlag(gResized); }
bool consumeSuspend() { return consumeFlag(gSuspendRequested); }
bool consumeContinue() { return consumeFlag(gContinued); }

void stopProcess() {
    // Our SIGTSTP handler only sets a flag, so to actually stop we switch
    // SIGTSTP back to its default action ("stop the process") and send it to
    // ourselves. raise() doesn't return until something sends SIGCONT, which
    // is what the shell's `fg` command does. Then we put our handler back.
    setHandler(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
    setHandler(SIGTSTP, onSuspendSignal);
}

TerminalSession::TerminalSession() {
    hasOriginalInput_ = tcgetattr(STDIN_FILENO, &originalInput_) == 0;
    enter();
}

TerminalSession::~TerminalSession() {
    leave();
}

void TerminalSession::enter() {
    // Normally the terminal driver collects a whole line, echoing it as you
    // type, and only hands it to the program when you press Enter ("canonical"
    // mode). Turning off ICANON delivers each key right away, and turning off
    // ECHO stops typed keys from being printed over our picture.
    //
    // We leave ISIG on, so Ctrl+C and Ctrl+Z still send SIGINT and SIGTSTP and
    // our signal handlers still work. That's why this isn't fully raw mode
    // (cfmakeraw).
    if (hasOriginalInput_) {
        termios raw = originalInput_;
        raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
        // TCSAFLUSH applies the change after pending output is written, and
        // throws away anything typed before now.
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    writeToTerminal(std::string(kEnterAltScreen) + kHideCursor + kClearScreen + kEnableMouse);
}

void TerminalSession::leave() {
    // Undo in reverse order. If mouse reporting were left on, every click in
    // the shell afterwards would print escape-code garbage.
    writeToTerminal(std::string(kDisableMouse) + kShowCursor + kLeaveAltScreen);

    // TCSAFLUSH also throws away keys pressed but not yet read, so they don't
    // end up typed into the shell.
    if (hasOriginalInput_) tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalInput_);
}