// Maps an asset path (by basename) to its embedded buffer (bin2o from data/).
#pragma once
#include <string>

// PNG sprites: basename ("stand.png") -> embedded RGBA buffer.
bool asset_lookup(const char *path, const unsigned char **buf, unsigned *size);

// Text configs (assets.txt / levelN.txt): there is no filesystem on PS3, so the
// game's ifstream reads are redirected here. Returns the embedded file contents
// as a std::string (empty if not found); feed it to a std::istringstream.
std::string load_config(const std::string &path);
