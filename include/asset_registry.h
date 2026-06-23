// Maps an asset path (by basename) to its embedded buffer (bin2o from data/).
#pragma once
bool asset_lookup(const char *path, const unsigned char **buf, unsigned *size);
