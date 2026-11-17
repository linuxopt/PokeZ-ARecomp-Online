// The player's Switch keys files (prod.keys / title.keys) for the start screen's game-file setup (launcher.mm
// find_keys): telling them apart by content, and finding the ones an installed Switch emulator already holds, so a
// player who set one up doesn't have to copy them next to the dump. Only read, never copied or sent anywhere: the path
// goes to zaimport on this machine. Header-only (static) so tools/test_switch_keys.cpp tests it without the runtime.
#pragma once
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <vector>

// Told apart by what is in them, whatever they are named (prod.keys, prod.keys.txt - Windows' hidden extensions -,
// "prod (1).keys", keys.txt...): a file named *.keys / *.key / *.txt (under 1 MB) is KEYS_PROD with a header_key line,
// KEYS_TITLE when its lines are "<rights id> = <title key>" (32 hex digits each).
enum { KEYS_NONE, KEYS_PROD, KEYS_TITLE };
static bool keys_file_name(std::string n) {
    for (auto &ch : n) ch = (char)tolower((unsigned char)ch);
    auto ends = [&](const char *x) { size_t k = strlen(x); return n.size() > k && !n.compare(n.size() - k, k, x); };
    return ends(".keys") || ends(".key") || ends(".txt");
}
static bool hex32(const std::string &s) {
    if (s.size() != 32) return false;
    for (char c : s)
        if (!isxdigit((unsigned char)c)) return false;
    return true;
}
static std::string keys_read_lower(const std::string &path) {  // "" when not a small regular file
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > (1 << 20)) return "";
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return "";
    std::string s((size_t)st.st_size, 0);
    s.resize(fread(&s[0], 1, s.size(), f));
    fclose(f);
    for (auto &ch : s) ch = (char)tolower((unsigned char)ch);
    return s;
}
// Each "name = value" line, without its spaces
template <class F> static void keys_lines(const std::string &s, F each) {
    size_t at = 0;
    while (at < s.size()) {
        size_t end = s.find('\n', at);
        if (end == std::string::npos) end = s.size();
        std::string line;
        for (size_t i = at; i < end; i++)
            if (!isspace((unsigned char)s[i])) line += s[i];
        size_t eq = line.find('=');
        if (eq != std::string::npos) each(line.substr(0, eq), line.substr(eq + 1));
        at = end + 1;
    }
}
static int keys_kind(const std::string &path) {
    std::string s = keys_read_lower(path);
    if (s.empty()) return KEYS_NONE;
    if (s.find("header_key") != std::string::npos) return KEYS_PROD;
    int title = 0;
    keys_lines(s, [&](const std::string &k, const std::string &v) { title += hex32(k) && hex32(v); });
    return title ? KEYS_TITLE : KEYS_NONE;
}
// The newest master key in a prod.keys (master_key_XX, hex): which firmware it was dumped from. A game update needs
// the master key its NCAs were made with - keys from an older firmware fail in zaimport ("your prod.keys has no ...").
// -1: none (a header_key-only file).
static int prod_keys_generation(const std::string &path) {
    int best = -1;
    keys_lines(keys_read_lower(path), [&](const std::string &k, const std::string &v) {
        if (k.size() == 13 && !k.compare(0, 11, "master_key_") && isxdigit((unsigned char)k[11]) &&
            isxdigit((unsigned char)k[12]) && hex32(v))
            best = std::max(best, (int)strtol(k.c_str() + 11, NULL, 16));
    });
    return best;
}

// A keys file of that kind in dir (prod.keys / title.keys first), "" if none. bad: a keys-named file that is neither
// (for the error: the player sees which file was looked at).
static std::string keys_in_dir(const std::string &dir, int kind, std::string *bad) {
    std::string named = dir + (kind == KEYS_PROD ? "/prod.keys" : "/title.keys");
    if (keys_kind(named) == kind) return named;
    DIR *d = opendir(dir.c_str());
    if (!d) return "";
    std::vector<std::string> names;
    while (struct dirent *e = readdir(d))
        if (e->d_name[0] != '.' && keys_file_name(e->d_name)) names.push_back(e->d_name);
    closedir(d);
    std::sort(names.begin(), names.end());
    for (auto &n : names) {
        int k = keys_kind(dir + "/" + n);
        if (k == kind) return dir + "/" + n;
        if (k == KEYS_NONE && bad && bad->empty()) *bad = n;
    }
    return "";
}

// Where Switch emulators keep the keys they were given (prod.keys and title.keys side by side), in order of
// preference when two hold keys of the same firmware. Ryujinx (and its Ryubing / Ryujinx-Mirror continuations: same
// folder) keeps them in <config>/Ryujinx/system; yuzu and its forks in <data>/<name>/keys - torzu, suyu, sudachi,
// citron and eden kept yuzu's layout under their own names. Flatpaks (Flathub, the Steam Deck's Discover) keep their
// own ~/.var/app/<id>/{config,data}: every installed app id is looked in, so a renamed app id still counts.
// RetroDECK keeps one copy for its Switch emulators in ~/retrodeck/bios/switch/keys. ~/.switch: hactool's and
// Lockpick's own place.
static const char *const k_yuzu_family[] = {"eden", "citron", "sudachi", "suyu", "torzu", "yuzu"};
static std::vector<std::string> emulator_key_dirs() {
    std::vector<std::string> dirs;
    auto emulators = [&](const std::string &config, const std::string &data) {
        dirs.push_back(config + "/Ryujinx/system");
        for (const char *n : k_yuzu_family) dirs.push_back(data + "/" + n + "/keys");
    };
#ifdef _WIN32
    if (const char *a = getenv("APPDATA"); a && *a) emulators(a, a);  // (%APPDATA%\Ryujinx\system, %APPDATA%\yuzu\keys)
    if (const char *h = getenv("USERPROFILE"); h && *h) dirs.push_back(std::string(h) + "/.switch");
#else
    const char *home = getenv("HOME");
    std::string h = home && *home ? home : "";
#ifdef __APPLE__
    if (!h.empty()) emulators(h + "/Library/Application Support", h + "/Library/Application Support");
#else
    const char *xc = getenv("XDG_CONFIG_HOME"), *xd = getenv("XDG_DATA_HOME");  // (relative values are invalid: ignored)
    std::string config = xc && *xc == '/' ? xc : h.empty() ? "" : h + "/.config";
    std::string data = xd && *xd == '/' ? xd : h.empty() ? "" : h + "/.local/share";
    if (!config.empty() && !data.empty()) emulators(config, data);
    if (!h.empty()) {
        std::vector<std::string> apps;
        if (DIR *d = opendir((h + "/.var/app").c_str())) {
            while (struct dirent *e = readdir(d))
                if (e->d_name[0] != '.') apps.push_back(h + "/.var/app/" + e->d_name);
            closedir(d);
        }
        std::sort(apps.begin(), apps.end());
        for (auto &a : apps) emulators(a + "/config", a + "/data");
        dirs.push_back(h + "/retrodeck/bios/switch/keys");
    }
#endif
    if (!h.empty()) dirs.push_back(h + "/.switch");
#endif
    return dirs;
}

// The emulators' prod.keys from the newest firmware (several emulators set up at different times: the oldest may
// lack the update's master key), "" if none. found: how many were seen (the log line).
static std::string find_emulator_keys(const std::vector<std::string> &dirs, int *found = NULL) {
    std::string best;
    int best_gen = -2, n = 0;
    for (auto &d : dirs) {
        std::string k = keys_in_dir(d, KEYS_PROD, NULL);
        if (k.empty()) continue;
        n++;
        int g = prod_keys_generation(k);
        if (g > best_gen) best = k, best_gen = g;  // (strictly newer: the earlier folder wins a tie)
    }
    if (found) *found = n;
    return best;
}
