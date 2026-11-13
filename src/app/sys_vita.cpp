// PS Vita (newlib + SceIofilemgr) side of sys.h.
// Paths are std::wstring everywhere in the app; they are converted to UTF-8 for the C library.
#include "sys.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

std::string to_utf8(const std::wstring& w) {
    std::string s;
    for (wchar_t wc : w) {
        uint32_t c = (uint32_t)wc;
        if (c < 0x80) s += (char)c;
        else if (c < 0x800) { s += (char)(0xC0 | (c >> 6)); s += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { s += (char)(0xE0 | (c >> 12)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
        else { s += (char)(0xF0 | (c >> 18)); s += (char)(0x80 | ((c >> 12) & 0x3F)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
    }
    return s;
}

std::wstring from_utf8(const std::string& s) {
    std::wstring w;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        int n = c < 0x80 ? 0 : c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
        if (n < 0) { w += (wchar_t)0xFFFD; i++; continue; }
        uint32_t cp = n ? c & (0x3Fu >> n) : c;
        size_t j = i + 1;
        for (int k = 0; k < n && j < s.size(); k++, j++) cp = (cp << 6) | ((unsigned char)s[j] & 0x3F);
        w += (wchar_t)cp;
        i = j;
    }
    return w;
}

std::wstring path_join(const std::wstring& dir, const std::wstring& name) { return dir + L"/" + name; }

std::wstring file_name(const std::wstring& path) {
    size_t s = path.find_last_of(L"\\/");
    return s == std::wstring::npos ? path : path.substr(s + 1);
}

FILE* open_file(const std::wstring& path, const char* mode) { return fopen(to_utf8(path).c_str(), mode); }

bool read_file(const std::wstring& path, std::vector<uint8_t>& out) {
    FILE* f = open_file(path, "rb");
    if (!f) return false;
    uint64_t n = file_size(f);
    out.resize((size_t)n);
    bool ok = n == 0 || fread(out.data(), 1, (size_t)n, f) == n;
    fclose(f);
    return ok;
}

bool read_text(const std::wstring& path, std::string& out) {
    std::vector<uint8_t> d;
    if (!read_file(path, d)) return false;
    out.assign(d.begin(), d.end());
    return true;
}

bool write_file(const std::wstring& path, const void* data, size_t n) {
    // Write to a temporary file and rename it over the old one, so a crash mid-write never leaves half a file.
    const std::wstring tmp = path + L".tmp";
    FILE* f = open_file(tmp, "wb");
    if (!f) return false;
    bool ok = n == 0 || fwrite(data, 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(to_utf8(tmp).c_str());
        return false;
    }
    // sceIofilemgr's rename() refuses to replace an existing file, so drop the old one first.
    remove(to_utf8(path).c_str());
    if (rename(to_utf8(tmp).c_str(), to_utf8(path).c_str()) != 0) {
        remove(to_utf8(tmp).c_str());
        return false;
    }
    return true;
}

bool file_seek(FILE* f, uint64_t pos) { return fseek(f, (long)pos, SEEK_SET) == 0; }

uint64_t file_size(FILE* f) {
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, cur, SEEK_SET);
    return n < 0 ? 0 : (uint64_t)n;
}

void make_dir(const std::wstring& path) { mkdir(to_utf8(path).c_str(), 0755); }

bool list_dir(const std::wstring& dir, std::vector<DirEntry>& out) {
    out.clear();
    const std::string d = to_utf8(dir);
    DIR* h = opendir(d.c_str());
    if (!h) return false;
    while (dirent* e = readdir(h)) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        struct stat st;
        if (stat((d + "/" + e->d_name).c_str(), &st) != 0) continue;
        DirEntry de;
        de.name = from_utf8(e->d_name);
        de.size = (uint64_t)st.st_size;
        de.mtime = (uint64_t)st.st_mtime;
        de.is_dir = S_ISDIR(st.st_mode);
        out.push_back(std::move(de));
    }
    closedir(h);
    return true;
}

// Where settings, save states and the ROMs live. Vita flash memory is always writable from the app.
std::wstring data_dir() {
    static std::wstring dir = L"ux0:data/MickeyAndDonald";
    return dir;
}

// The VPK itself (the folder the eboot is in).
std::wstring exe_dir() { return L"app0:"; }

// ROM folders, in search order: the data folder's "roms", the data folder, and next to the eboot.
std::vector<std::wstring> rom_dirs() {
    return {
        path_join(data_dir(), L"roms"),
        data_dir(),
        L"app0:roms",
        L"app0:",
    };
}

void open_url(const char*) {}
