#pragma once

namespace Platform
{
    enum class Key
    {
        KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,

        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
        KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,

        KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

        KEY_SPACE, KEY_ESCAPE, KEY_ENTER, KEY_TAB,
        KEY_UNKNOWN
    };

    enum class MouseButton
    {
        MOUSE_LEFT, MOUSE_MIDDLE, MOUSE_RIGHT,
        MOUSE_BUTTON_1, MOUSE_BUTTON_2, MOUSE_BUTTON_3, MOUSE_BUTTON_4, MOUSE_BUTTON_5,
        MOUSE_UNKNOWN
    };

    enum class Action
    {
        PRESS, RELEASE, REPEAT,
        UNKNOWN
    };

    enum class CursorMode
    {
        NORMAL,   // Cursor is visible and not being captured by the window.
        HIDDEN,   // Cursor is hidden when hovering over the window and not being captured by it.
        CAPTURED, // Cursor is hidden and captured by the window.
        UNKNOWN
    };
}