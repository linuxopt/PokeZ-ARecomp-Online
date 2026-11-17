// runtime/ui/switch_keys.h: keys told apart by content, and the installed emulators' prod.keys found (newest firmware
// first) in a made-up home folder. Linux / macOS layout (the Windows one is %APPDATA% with the same names).
//   clang++ -std=c++17 -Iruntime/ui tools/test_switch_keys.cpp -o build/test_switch_keys && build/test_switch_keys
#include "switch_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static std::string root;
static void put(const std::string &rel, const std::string &text) {
    std::string p = root + "/" + rel;
    std::string cmd = "mkdir -p '" + p.substr(0, p.rfind('/')) + "'";
    if (system(cmd.c_str())) abort();
    FILE *f = fopen(p.c_str(), "wb");
    fputs(text.c_str(), f);
    fclose(f);
}
static std::string prod(int newest) {  // a made-up prod.keys from firmware generation <newest> (not real keys)
    std::string s = "header_key = " + std::string(64, '1') + "\n";
    for (int g = 0; g <= newest; g++) {
        char line[96];
        snprintf(line, sizeof line, "master_key_%02x = %s\n", g, std::string(32, 'a').c_str());
        s += line;
    }
    return s;
}

int main() {
    char tmpl[] = "/tmp/switch-keys-XXXXXX";
    if (!mkdtemp(tmpl)) return 1;
    root = tmpl;
    std::string title = std::string(32, 'b') + " = " + std::string(32, 'c') + "\n";

    // content, not names
    put("kinds/prod.keys.txt", prod(3));
    put("kinds/title.keys", title);
    put("kinds/notes.txt", "hello\n");
    CHECK(keys_kind(root + "/kinds/prod.keys.txt") == KEYS_PROD);
    CHECK(keys_kind(root + "/kinds/title.keys") == KEYS_TITLE);
    CHECK(keys_kind(root + "/kinds/notes.txt") == KEYS_NONE);
    CHECK(prod_keys_generation(root + "/kinds/prod.keys.txt") == 3);
    CHECK(prod_keys_generation(root + "/kinds/title.keys") == -1);
    std::string bad;
    CHECK(keys_in_dir(root + "/kinds", KEYS_PROD, &bad) == root + "/kinds/prod.keys.txt");

    // no emulator: nothing
    setenv("HOME", (root + "/home").c_str(), 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    CHECK(find_emulator_keys(emulator_key_dirs()).empty());

    // one emulator (Ryujinx, native)
    put("home/.config/Ryujinx/system/prod.keys", prod(0x10));
    put("home/.config/Ryujinx/system/title.keys", title);
    int found = 0;
    CHECK(find_emulator_keys(emulator_key_dirs(), &found) == root + "/home/.config/Ryujinx/system/prod.keys");
    CHECK(found == 1);

    // a newer firmware's keys in Eden win over Ryujinx's older ones
    put("home/.local/share/eden/keys/prod.keys", prod(0x13));
    CHECK(find_emulator_keys(emulator_key_dirs(), &found) == root + "/home/.local/share/eden/keys/prod.keys");
    CHECK(found == 2);

    // a Flatpak (any app id) with the newest
    put("home/.var/app/org.example.Citron/data/citron/keys/prod.keys", prod(0x14));
    CHECK(find_emulator_keys(emulator_key_dirs()) == root + "/home/.var/app/org.example.Citron/data/citron/keys/prod.keys");
    put("home/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/system/prod.keys", prod(0x15));
    CHECK(find_emulator_keys(emulator_key_dirs()) ==
          root + "/home/.var/app/io.github.ryubing.Ryujinx/config/Ryujinx/system/prod.keys");

    // same firmware: the earlier folder in the list (native Ryujinx) wins the tie
    put("home/.config/Ryujinx/system/prod.keys", prod(0x15));
    CHECK(find_emulator_keys(emulator_key_dirs()) == root + "/home/.config/Ryujinx/system/prod.keys");

    // RetroDECK, and the XDG folders honoured
    put("xdgdata/yuzu/keys/prod.keys", prod(0x16));
    setenv("XDG_DATA_HOME", (root + "/xdgdata").c_str(), 1);
    CHECK(find_emulator_keys(emulator_key_dirs()) == root + "/xdgdata/yuzu/keys/prod.keys");
    setenv("XDG_DATA_HOME", "relative/ignored", 1);
    put("home/retrodeck/bios/switch/keys/prod.keys", prod(0x17));
    CHECK(find_emulator_keys(emulator_key_dirs()) == root + "/home/retrodeck/bios/switch/keys/prod.keys");

    // a non-keys file in an emulator folder is skipped, not an error
    put("home/.local/share/suyu/keys/prod.keys", "not keys\n");
    CHECK(find_emulator_keys(emulator_key_dirs(), &found) == root + "/home/retrodeck/bios/switch/keys/prod.keys");
    CHECK(found == 5);

    std::string cmd = "rm -rf '" + root + "'";
    if (system(cmd.c_str())) fails++;
    if (fails) return 1;
    printf("switch_keys: all checks passed\n");
    return 0;
}
