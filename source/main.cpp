/*
 * PS3 Mega Mario - entry point
 *
 * PS3 homebrew port of "mega-mario" (a C++/SFML ECS platformer). From Phase 5 on
 * this is the original game's entry point unchanged: GameEngine owns the window,
 * scene stack, and main loop. All platform bring-up (tiny3d / ya2d / pad / fonts /
 * XMB callback) happens inside the SFML shim's RenderWindow::create()
 * (source/sfml_backend.cpp), and assets/levels load from embedded memory rather
 * than disk (source/asset_registry.cpp). See docs/PATTERNS.md and todo/ROADMAP.md.
 */
#include "GameEngine.h"
#include "audio.h"

int main()
{
	GameEngine game("../bin/assets.txt");
	game.run();
	audio_shutdown();   // stop MikMod cleanly on exit
	return 0;
}
