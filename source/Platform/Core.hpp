#pragma once

#include "InputDefinitions.hpp"

#include "EventDispatcher.hpp"

#include <array>
#include <limits>
#include <type_traits>
#include <utility>

typedef struct GLFWwindow GLFWwindow;

namespace Platform
{
    class Window
    {
        friend class Core;
    public:
        void setInputMode(const CursorMode& pCursorMode);
        void requestClose();

        void set_VSync(bool p_enabled);
        bool get_VSync() const { return m_VSync; };

        std::pair<int, int> size() const { return m_fullscreen ? m_size_fullscreen : m_size_windowed; };
        void set_size(std::pair<int, int> p_new_size);

        std::pair<int, int> position() const { return m_fullscreen ? m_position_fullscreen : m_position_windowed; };
        void set_position(std::pair<int, int> p_new_position);

        void toggle_fullscreen();

        bool capturingMouse() const { return mCapturingMouse != mCapturedChangedThisFrame; };

        float get_aspect_ratio() const { return m_aspect_ratio; };

    private:
        Window(const int& pWidth, const int& pHeight);
        ~Window();

        std::pair<int, int> m_size_fullscreen;     // the most up-to-date size of the window in fullscreen.
        std::pair<int, int> m_position_fullscreen; // the most up-to-date position of the window in fullscreen.
        std::pair<int, int> m_size_windowed;       // the most up-to-date size of the window in windowed.
        std::pair<int, int> m_position_windowed;   // the most up-to-date position of the window in windowed.

        bool m_fullscreen;
        float m_aspect_ratio;
        bool m_VSync; // Whether VSync is on for the window.
        bool mCapturingMouse; // Is the mouse hidden and input is restricted to this window.
        bool mCapturedChangedThisFrame; // Has mCapturingMouse changed on this pollEventCycle. Useful for avoiding same cycle input clashes.
        GLFWwindow* mHandle;
    };


    // Core abstracts away any external libs we are using.
    // Core provides a platform agnostic handler for Windows, Input and Graphics contexts.
    class Core
    {
    public:
        // HELPERS
        static std::pair<int,int> getPrimaryMonitorResolution();

        // INIT
        static void initialise();
        static void cleanup();

        // GRAPHICS
        static void swapBuffers();
        static void startImGuiFrame();
        static void endImGuiFrame();

        static Window& getWindow() { return *mPrimaryWindow; };
        static bool hasWindow()    { return mPrimaryWindow;  };

        // INPUT
        static void pollEvents();
        // Is the mouse hovering over any UI elements. Click events during this state will generally be absorbed by the UI.
        static bool UICapturingMouse();
        // Is the UI capturing keyboard input, such when a user clicks on an input box.
        static bool UICapturingKeyboard();

        // Is p_key currently down
        static bool is_key_down(const Key& p_key);

        // Returns the cursor screen-coordinates, relative to the upper-left corner of the primary window.
        // If the cursor is captured by the window the values returned can be negative (Window::capturingMouse).
        static std::pair<float, float> getCursorPosition();

        // EVENTS
        static inline Utility::EventDispatcher<const Key&> mKeyPressEvent;
        static inline Utility::EventDispatcher<const MouseButton&, const Action&> mMouseButtonEvent;
        static inline Utility::EventDispatcher<const float&, const float&> mMouseMoveEvent;
        static inline Utility::EventDispatcher<const int&, const int&> mWindowResizeEvent;

    private:
        constexpr static auto Max_Key_Index = std::numeric_limits<std::underlying_type_t<Key>>::max();
        static inline std::array<bool, Max_Key_Index> keys_pressed = { false };

        static inline Window* mPrimaryWindow = nullptr;
        static inline double mLastXPosition = -1.0;
        static inline double mLastYPosition = -1.0;

        // GLFW callbacks
        static void GLFW_errorCallback(int pError, const char* pDescription);
        static void GLFW_windowCloseCallback(GLFWwindow* pWindow); // Called when the window title bar X is pressed.
        static void GLFW_keyPressCallback(GLFWwindow* pWindow, int pKey, int pScancode, int pAction, int pMode); // Called when a key is pressed in glfwPollEvents.
        static void GLFW_mouseMoveCallback(GLFWwindow* pWindow, double pNewXPosition, double pNewYPosition);
        static void GLFW_mouseButtonCallback(GLFWwindow* pWindow, int pButton, int pAction, int pModifiers);
        static void GLFW_windowResizeCallback(GLFWwindow* pWindow, int pWidth, int pHeight);
        static void GLFW_window_move_callback(GLFWwindow* p_window, int p_x_position, int p_y_position);

        // GLFW to platform conversion funcs
        static Key GLFW_getKey(const int& pKeyInput);
        static MouseButton GLFW_getMouseButton(const int& pMouseButton);
        static Action GLFW_getAction(const int& pAction);
    };
}