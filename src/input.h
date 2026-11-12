#pragma once

#include <cstddef>
#include <string>
#include <vector>

// One thing the user did: a key press or a mouse action.
struct InputEvent {
    enum class Type {
        Char,         // an ordinary key; the character is in `ch`
        Up,           // arrow keys
        Down,
        Left,
        Right,
        Escape,       // the Esc key on its own
        MousePress,   // a mouse button went down at (x, y)
        MouseDrag,    // the mouse moved to (x, y) with a button held
        MouseRelease, // the button came back up at (x, y)
        ScrollUp,     // the scroll wheel (or a trackpad scroll) at (x, y)
        ScrollDown,
    };

    Type type;
    char ch = 0; // for Char
    int x = 0;   // for mouse events: the cell, counted from 0 at the top-left
    int y = 0;
};

// Turns raw bytes from the terminal into events. Most keys are one byte, but
// special keys and mouse actions arrive as escape sequences: the up arrow is
// ESC '[' 'A', and a click is ESC '[' '<' 0 ';' x ';' y 'M'.
//
// If the bytes end partway through an escape sequence, parsing stops there and
// `consumed` says how many bytes were used, so the caller can keep the rest
// and try again once more bytes arrive.
std::vector<InputEvent> parseInput(const std::string& bytes, std::size_t& consumed);

// Reads keyboard and mouse input from stdin without ever blocking.
class InputReader {
public:
    // Returns the events for everything complete that's waiting on stdin, or
    // an empty list if there's nothing. Never waits for input.
    std::vector<InputEvent> readEvents();

private:
    std::string pending_; // the start of an escape sequence that hasn't fully arrived
};
