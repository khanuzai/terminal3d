#pragma once

#include <string>

#include <termios.h>

// Size of the terminal window, measured in character cells.
struct TermSize {
    int cols;
    int rows;
};

// Asks the OS how big the terminal is. Falls back to 80x24 (the classic
// terminal size) if stdout isn't a terminal, e.g. when output is piped.
TermSize terminalSize();

// Writes the whole string to stdout using the raw write() system call.
// Sending a frame in one big write, instead of many small ones, keeps the
// terminal from showing a half-drawn picture.
void writeToTerminal(const std::string& data);

// Writes one complete frame, wrapped in "synchronized output" markers. Terminals
// that support them (iTerm2, kitty, WezTerm, Alacritty, foot, ...) hold the
// screen still until the whole frame has arrived and then show it at once,
// which removes any remaining tearing. Other terminals ignore the markers.
void presentFrame(const std::string& frame);

// Erases the whole screen. Needed after a resize, when the old frame's
// layout no longer lines up with the new size.
void clearTerminal();

// Installs handlers for Ctrl+C (SIGINT), SIGTERM and window resizes (SIGWINCH).
// The handlers only set flags; the main loop checks them with the functions
// below and reacts in normal code, where it's safe to do anything.
void installSignalHandlers();

// True once Ctrl+C or SIGTERM has been received.
bool quitRequested();

// True if the window was resized since the last call. Calling it clears the flag.
bool consumeResize();

// RAII guard for the terminal's state. Creating one:
//   - switches to the "alternate screen" (a separate full-screen buffer,
//     which is what vim and htop use) and hides the cursor
//   - puts keyboard input into "raw" mode: each key arrives as soon as it's
//     pressed, without waiting for Enter, and isn't echoed on screen
// Destroying it undoes all of that, so the user's shell comes back exactly as
// it was.
class TerminalSession {
public:
    TerminalSession();
    ~TerminalSession();

    // Copying would mean restoring the terminal twice, so forbid it.
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

private:
    termios originalInput_{};    // input settings from before we changed them
    bool restoreInput_ = false;  // false if stdin isn't a terminal
};
