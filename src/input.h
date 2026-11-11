#pragma once

#include <cstddef>
#include <string>
#include <vector>

// One thing the user did. For now that's a key press; mouse events come later.
struct InputEvent {
    enum class Type {
        Char,   // an ordinary key; the character is in `ch`
        Up,     // arrow keys
        Down,
        Left,
        Right,
        Escape, // the Esc key on its own
    };

    Type type;
    char ch = 0;
};

// Turns raw bytes from the terminal into events. Most keys are one byte, but
// special keys arrive as escape sequences: the up arrow is ESC '[' 'A'.
//
// If the bytes end partway through an escape sequence, parsing stops there and
// `consumed` says how many bytes were used, so the caller can keep the rest
// and try again once more bytes arrive.
std::vector<InputEvent> parseInput(const std::string& bytes, std::size_t& consumed);

// Reads keyboard input from stdin without ever blocking.
class InputReader {
public:
    // Returns the events for every complete key press waiting on stdin, or an
    // empty list if there's nothing. Never waits for input.
    std::vector<InputEvent> readEvents();

private:
    std::string pending_; // the start of an escape sequence that hasn't fully arrived
};
