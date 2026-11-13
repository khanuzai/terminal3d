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
// Returns false if the terminal can't be written to any more (it was closed).
bool writeToTerminal(const std::string& data);

// Writes one complete frame, wrapped in "synchronized output" markers. Terminals
// that support them (iTerm2, kitty, WezTerm, Alacritty, foot, ...) hold the
// screen still until the whole frame has arrived and then show it at once,
// which removes any remaining tearing. Other terminals ignore the markers.
// Returns false if the terminal can't be written to any more.
bool presentFrame(const std::string& frame);

// Erases the whole screen. Needed after a resize, when the old frame's
// layout no longer lines up with the new size.
void clearTerminal();

// Installs handlers for:
//   SIGINT (Ctrl+C), SIGTERM, SIGHUP (terminal closed)  -> quit
//   SIGWINCH (window resized)                           -> resize
//   SIGTSTP (Ctrl+Z)                                    -> suspend
//   SIGCONT (resumed, e.g. by `fg`)                     -> continue
// The handlers only set flags; the main loop checks them with the functions
// below and reacts in normal code, where it's safe to do anything.
void installSignalHandlers();

// True once a quit signal has been received.
bool quitRequested();

// Each of these returns true if its signal arrived since the last call, and
// clears the flag.
bool consumeResize();
bool consumeSuspend();
bool consumeContinue();

// Stops the whole process, exactly as Ctrl+Z normally would, and returns
// once it's been resumed. Call TerminalSession::leave() first so the shell
// gets a usable terminal while we're stopped.
void stopProcess();

// RAII guard for the terminal's state. Creating one:
//   - switches to the "alternate screen" (a separate full-screen buffer,
//     which is what vim and htop use) and hides the cursor
//   - puts keyboard input into "raw" mode: each key arrives as soon as it's
//     pressed, without waiting for Enter, and isn't echoed on screen
//   - asks the terminal to report mouse clicks, drags and the scroll wheel
//     as input
// Destroying it undoes all of that, so the user's shell comes back exactly as
// it was.
class TerminalSession {
public:
    TerminalSession();
    ~TerminalSession();

    // Copying would mean restoring the terminal twice, so forbid it.
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

    // Applies all of our terminal settings. Safe to call again at any time,
    // e.g. after being resumed, when the shell may have changed them.
    void enter();

    // Puts the terminal back the way it was before we started.
    void leave();

private:
    termios originalInput_{};       // input settings from before we changed them
    bool hasOriginalInput_ = false; // false if stdin isn't a terminal
};
