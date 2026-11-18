#include <string>

#include "input.h"
#include "test.h"

using Type = InputEvent::Type;

TEST(input_plain_keys_become_char_events) {
    std::size_t consumed = 0;
    const auto events = parseInput("q +", consumed);
    CHECK_EQ(consumed, 3u);
    REQUIRE(events.size() == 3u);
    CHECK_EQ(events[0].type, Type::Char);
    CHECK_EQ(events[0].ch, 'q');
    CHECK_EQ(events[1].ch, ' ');
    CHECK_EQ(events[2].ch, '+');
}

TEST(input_arrows_in_both_encodings) {
    // Terminals send ESC [ A normally, ESC O A in "application" mode, and add
    // parameters when a modifier is held: ESC [ 1 ; 5 A is Ctrl+Up.
    std::size_t consumed = 0;
    const std::string bytes = "\x1b[A\x1bOB\x1b[C\x1b[1;5D";
    const auto events = parseInput(bytes, consumed);
    CHECK_EQ(consumed, bytes.size()); // all of it: no sequence left half-read
    REQUIRE(events.size() == 4u);
    CHECK_EQ(events[0].type, Type::Up);
    CHECK_EQ(events[1].type, Type::Down);
    CHECK_EQ(events[2].type, Type::Right);
    CHECK_EQ(events[3].type, Type::Left);
}

TEST(input_lone_escape_is_the_escape_key) {
    std::size_t consumed = 0;
    auto events = parseInput("\x1b", consumed);
    REQUIRE(events.size() == 1u);
    CHECK_EQ(events[0].type, Type::Escape);
    CHECK_EQ(consumed, 1u);

    // ESC followed by an ordinary key: Esc, then the key.
    events = parseInput("\x1bq", consumed);
    REQUIRE(events.size() == 2u);
    CHECK_EQ(events[0].type, Type::Escape);
    CHECK_EQ(events[1].ch, 'q');
}

TEST(input_incomplete_sequence_is_left_for_later) {
    // The first half of a mouse report: parsing stops before it, so the
    // caller keeps those bytes until the rest arrives.
    std::size_t consumed = 0;
    auto events = parseInput("w\x1b[<0;1", consumed);
    REQUIRE(events.size() == 1u);
    CHECK_EQ(events[0].ch, 'w');
    CHECK_EQ(consumed, 1u);

    events = parseInput("\x1b[<0;10;5M", consumed);
    REQUIRE(events.size() == 1u);
    CHECK_EQ(events[0].type, Type::MousePress);
}

TEST(input_mouse_press_drag_release_with_zero_based_cells) {
    std::size_t consumed = 0;
    const auto events = parseInput("\x1b[<0;10;5M\x1b[<32;11;6M\x1b[<0;11;6m", consumed);
    REQUIRE(events.size() == 3u);
    CHECK_EQ(events[0].type, Type::MousePress);
    CHECK_EQ(events[0].x, 9); // terminals count from 1, we count from 0
    CHECK_EQ(events[0].y, 4);
    CHECK_EQ(events[1].type, Type::MouseDrag);
    CHECK_EQ(events[1].x, 10);
    CHECK_EQ(events[2].type, Type::MouseRelease);
}

TEST(input_scroll_wheel_ignoring_modifiers) {
    std::size_t consumed = 0;
    // 64 = wheel up, 65 = wheel down, +16 = Ctrl held.
    const auto events = parseInput("\x1b[<64;1;1M\x1b[<65;1;1M\x1b[<80;1;1M", consumed);
    REQUIRE(events.size() == 3u);
    CHECK_EQ(events[0].type, Type::ScrollUp);
    CHECK_EQ(events[1].type, Type::ScrollDown);
    CHECK_EQ(events[2].type, Type::ScrollUp);
}

TEST(input_unknown_and_malformed_sequences_are_skipped) {
    std::size_t consumed = 0;
    // F5 (ESC [ 15 ~), a mouse report with a missing number, sideways
    // scrolling (66), then a real key.
    const std::string bytes = "\x1b[15~\x1b[<0;5M\x1b[<66;1;1Mx";
    const auto events = parseInput(bytes, consumed);
    CHECK_EQ(consumed, bytes.size());
    REQUIRE(events.size() == 1u);
    CHECK_EQ(events[0].ch, 'x');
}
