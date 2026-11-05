#pragma once

#include <string>

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

// RAII guard for the terminal's state. Creating one switches to the
// "alternate screen" (a separate full-screen buffer, which is what vim and htop
// use) and hides the cursor. Destroying it undoes both, so the user's shell
// comes back exactly as it was.
class TerminalSession {
public:
    TerminalSession();
    ~TerminalSession();

    // Copying would mean restoring the terminal twice, so forbid it.
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
};
