// GENERATED: registry of embedded PNG assets (basename -> bin2o buffer).
#include "asset_registry.h"
#include <cstring>

extern "C" {
// Text configs (assets.txt / levelN.txt), embedded as data/*_txt.bin.
extern const unsigned char assets_txt_bin[]; extern const unsigned int assets_txt_bin_size;
extern const unsigned char level1_txt_bin[]; extern const unsigned int level1_txt_bin_size;
extern const unsigned char level2_txt_bin[]; extern const unsigned int level2_txt_bin_size;
extern const unsigned char level3_txt_bin[]; extern const unsigned int level3_txt_bin_size;
extern const unsigned char bigbush_png[]; extern const unsigned int bigbush_png_size;
extern const unsigned char bigcloud_png[]; extern const unsigned int bigcloud_png_size;
extern const unsigned char bigpipe_png[]; extern const unsigned int bigpipe_png_size;
extern const unsigned char block_png[]; extern const unsigned int block_png_size;
extern const unsigned char brick_png[]; extern const unsigned int brick_png_size;
extern const unsigned char bullet_png[]; extern const unsigned int bullet_png_size;
extern const unsigned char bush_png[]; extern const unsigned int bush_png_size;
extern const unsigned char coin_png[]; extern const unsigned int coin_png_size;
extern const unsigned char explosion_png[]; extern const unsigned int explosion_png_size;
extern const unsigned char flag_png[]; extern const unsigned int flag_png_size;
extern const unsigned char flagpole_png[]; extern const unsigned int flagpole_png_size;
extern const unsigned char flagtop_png[]; extern const unsigned int flagtop_png_size;
extern const unsigned char ground_png[]; extern const unsigned int ground_png_size;
extern const unsigned char jump_png[]; extern const unsigned int jump_png_size;
extern const unsigned char qshade_png[]; extern const unsigned int qshade_png_size;
extern const unsigned char question1_png[]; extern const unsigned int question1_png_size;
extern const unsigned char question2_png[]; extern const unsigned int question2_png_size;
extern const unsigned char run_png[]; extern const unsigned int run_png_size;
extern const unsigned char smallbush_png[]; extern const unsigned int smallbush_png_size;
extern const unsigned char smallcloud_png[]; extern const unsigned int smallcloud_png_size;
extern const unsigned char stand_png[]; extern const unsigned int stand_png_size;
extern const unsigned char tallpipe_png[]; extern const unsigned int tallpipe_png_size;
}

namespace {
struct Entry { const char *name; const unsigned char *buf; const unsigned int *size; };
const Entry kAssets[] = {
    { "bigbush.png", bigbush_png, &bigbush_png_size },
    { "bigcloud.png", bigcloud_png, &bigcloud_png_size },
    { "bigpipe.png", bigpipe_png, &bigpipe_png_size },
    { "block.png", block_png, &block_png_size },
    { "brick.png", brick_png, &brick_png_size },
    { "bullet.png", bullet_png, &bullet_png_size },
    { "bush.png", bush_png, &bush_png_size },
    { "coin.png", coin_png, &coin_png_size },
    { "explosion.png", explosion_png, &explosion_png_size },
    { "flag.png", flag_png, &flag_png_size },
    { "flagpole.png", flagpole_png, &flagpole_png_size },
    { "flagtop.png", flagtop_png, &flagtop_png_size },
    { "ground.png", ground_png, &ground_png_size },
    { "jump.png", jump_png, &jump_png_size },
    { "qshade.png", qshade_png, &qshade_png_size },
    { "question1.png", question1_png, &question1_png_size },
    { "question2.png", question2_png, &question2_png_size },
    { "run.png", run_png, &run_png_size },
    { "smallbush.png", smallbush_png, &smallbush_png_size },
    { "smallcloud.png", smallcloud_png, &smallcloud_png_size },
    { "stand.png", stand_png, &stand_png_size },
    { "tallpipe.png", tallpipe_png, &tallpipe_png_size },
};
}

bool asset_lookup(const char *path, const unsigned char **buf, unsigned *size) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    for (const Entry &e : kAssets)
        if (strcmp(e.name, base) == 0) { *buf = e.buf; *size = *e.size; return true; }
    return false;
}

// Text configs: map a basename ("assets.txt"/"level1.txt") to its embedded blob.
namespace {
const Entry kConfigs[] = {
    { "assets.txt", assets_txt_bin, &assets_txt_bin_size },
    { "level1.txt", level1_txt_bin, &level1_txt_bin_size },
    { "level2.txt", level2_txt_bin, &level2_txt_bin_size },
    { "level3.txt", level3_txt_bin, &level3_txt_bin_size },
};
}

std::string load_config(const std::string &path) {
    const char *base = strrchr(path.c_str(), '/');
    base = base ? base + 1 : path.c_str();
    for (const Entry &e : kConfigs)
        if (strcmp(e.name, base) == 0)
            return std::string((const char *)e.buf, *e.size);
    return std::string();
}
