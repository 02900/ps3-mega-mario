// Minimal SFML Audio shim for the PS3 port (MikMod backend lands in a later
// phase; these stubs just compile/link). See docs/PATTERNS.md §5, §7.
#pragma once
#include <string>

namespace sf {

class SoundBuffer {
public:
	bool loadFromFile(const std::string & /*path*/) { return true; }
};

class Sound {
public:
	const SoundBuffer *m_buffer = nullptr;
	Sound() {}
	explicit Sound(const SoundBuffer &b) : m_buffer(&b) {}
	void setBuffer(const SoundBuffer &b) { m_buffer = &b; }
	void play() {}
	void stop() {}
	void setLoop(bool) {}
	void setVolume(float) {}
};

class Music {
public:
	bool openFromFile(const std::string & /*path*/) { return true; }
	void play() {}
	void stop() {}
	void pause() {}
	void setLoop(bool) {}
	void setVolume(float) {}
};

}  // namespace sf
