#include <gtest/gtest.h>
#include "core/input.h"
#include "systems/input_system.h"

using namespace nk;

TEST(InputCodes, KeepsPortableKeyNamesStable) {
    EXPECT_EQ(KeyCode::PageUp, 0x21);
    EXPECT_EQ(KeyCode::PageDown, 0x22);
    EXPECT_EQ(KeyCode::PrintScreen, 0x2c);
    EXPECT_EQ(KeyCode::LSuper, 0x5b);
    EXPECT_EQ(KeyCode::RSuper, 0x5c);
    EXPECT_EQ(KeyCode::Decimal, 0x6e);
    EXPECT_EQ(KeyCode::Secimal, KeyCode::Decimal);
}

TEST(TextInput, IsExplicitFrameScopedAndUtf8Safe) {
    InputSystem::init();
    EXPECT_FALSE(Input::is_key_down(256));
    EXPECT_TRUE(Input::is_key_up(256));
    EXPECT_FALSE(InputSystem::process_text("ignored"));
    Input::set_text_input_enabled(true);
    ASSERT_TRUE(InputSystem::process_text("¡Hola ñ🙂"));
    EXPECT_EQ(Input::text_input().committed, "¡Hola ñ🙂");
    InputSystem::update(0);
    EXPECT_TRUE(Input::text_input().committed.empty());
    EXPECT_TRUE(Input::text_input_enabled());
    InputSystem::shutdown();
}

TEST(TextInput, KeepsPreeditAndDeletionSeparateFromCommittedText) {
    InputSystem::init(); Input::set_text_input_enabled(true);
    ASSERT_TRUE(InputSystem::process_preedit("áé", 0, 2));
    InputSystem::process_text_delete(3, 4);
    auto state = Input::text_input();
    EXPECT_EQ(state.preedit, "áé");
    EXPECT_EQ(state.selection_begin, 0);
    EXPECT_EQ(state.selection_end, 2);
    EXPECT_EQ(state.delete_before, 3);
    EXPECT_EQ(state.delete_after, 4);
    EXPECT_FALSE(InputSystem::process_preedit("á", 1, 1));
    InputSystem::update(0);
    state = Input::text_input();
    EXPECT_EQ(state.preedit, "áé");
    EXPECT_EQ(state.delete_before, 0);
    Input::set_text_input_enabled(false);
    InputSystem::process_text_delete(9, 9);
    EXPECT_EQ(Input::text_input().delete_before, 0);
    InputSystem::shutdown();
}

TEST(TextInput, TruncatesOnlyAtUnicodeScalarBoundaries) {
    InputSystem::init(); Input::set_text_input_enabled(true);
    char ascii[1023]; std::memset(ascii, 'a', sizeof(ascii));
    ASSERT_TRUE(InputSystem::process_text({ascii, sizeof(ascii)}));
    auto full = InputSystem::process_text("🙂");
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error(), text_input_error::capacity_exceeded);
    const auto state = Input::text_input();
    EXPECT_EQ(state.committed.length(), sizeof(ascii));
    EXPECT_TRUE(state.overflowed);
    EXPECT_FALSE(InputSystem::process_text("\xed\xa0\x80"));
    InputSystem::update(0);
    char oversized_preedit[256];
    std::memset(oversized_preedit, 'x', sizeof(oversized_preedit));
    EXPECT_FALSE(InputSystem::process_preedit(
        {oversized_preedit, sizeof(oversized_preedit)}, 0, 0));
    EXPECT_TRUE(Input::text_input().overflowed);
    InputSystem::shutdown();
}
