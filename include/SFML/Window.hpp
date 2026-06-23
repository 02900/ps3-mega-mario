// Minimal SFML Window shim for the PS3 port. See docs/PATTERNS.md §7.
#pragma once
#include "System.hpp"

namespace sf {

struct VideoMode {
	unsigned int width, height, bitsPerPixel;
	VideoMode() : width(0), height(0), bitsPerPixel(32) {}
	VideoMode(unsigned int w, unsigned int h, unsigned int bpp = 32)
		: width(w), height(h), bitsPerPixel(bpp) {}
};

namespace Style {
enum {
	None = 0, Titlebar = 1, Resize = 2, Close = 4,
	Fullscreen = 8, Default = Titlebar | Resize | Close
};
}

namespace Keyboard {
// SFML key codes (the game registers actions by these). Order matches SFML.
enum Key {
	Unknown = -1,
	A = 0, B, C, D, E, F, G, H, I, J, K, L, M,
	N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
	Escape, LControl, LShift, LAlt, LSystem, RControl, RShift, RAlt, RSystem,
	Menu, LBracket, RBracket, Semicolon, Comma, Period, Quote, Slash,
	Backslash, Tilde, Equal, Hyphen, Space, Enter, Backspace, Tab,
	PageUp, PageDown, End, Home, Insert, Delete,
	Add, Subtract, Multiply, Divide,
	Left, Right, Up, Down,
	Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
	Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
	F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15,
	Pause, KeyCount,
	Return = Enter   // SFML alias
};
inline bool isKeyPressed(Key) { return false; }  // backend fills this in
}  // namespace Keyboard

struct Event {
	enum EventType {
		Closed, Resized, LostFocus, GainedFocus, TextEntered,
		KeyPressed, KeyReleased, MouseWheelMoved, MouseWheelScrolled,
		MouseButtonPressed, MouseButtonReleased, MouseMoved,
		Count
	};
	struct KeyEvent { Keyboard::Key code; bool alt, control, shift, system; };
	struct SizeEvent { unsigned int width, height; };

	EventType type;
	KeyEvent  key;
	SizeEvent size;
};

}  // namespace sf
