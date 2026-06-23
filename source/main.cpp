/*
 * PS3 Mega Mario - entry point (SCAFFOLD STUB)
 *
 * PS3 homebrew port of "mega-mario" (a C++/SFML ECS platformer). This is a
 * placeholder: it compiles and links against the PSL1GHT C++ toolchain but does
 * NOT yet run the game.
 *
 * Bring-up plan (todo/ROADMAP.md):
 *   Phase 1 - init Tiny3D + ya2d, render a blank frame, clean XMB exit.
 *   Phase 2 - implement the SFML compatibility shim (sf:: over ya2d/pad/MikMod),
 *             then drop in the original src/ (downgraded C++20 -> C++17).
 *
 * See docs/PATTERNS.md (esp. §7) for the framework-shim porting approach.
 */

int main(int argc, const char *argv[])
{
	(void)argc;
	(void)argv;

	/* TODO(Phase 1): tiny3d/ya2d init + blank frame; then the SFML shim + ECS. */

	return 0;
}
