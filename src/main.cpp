#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "gsong.hpp"


#ifdef _MSC_VER
uint16_t swap(uint16_t v) { return  _byteswap_ushort(v); }
uint32_t swap(uint32_t v) { return  _byteswap_ulong(v); }
#else
uint16_t swap(uint16_t v) { return  __builtin_bswap16(v); }
uint32_t swap(uint32_t v) { return  __builtin_bswap32(v); }
#endif


#pragma pack(push, 1)
struct SidHeader {
    uint8_t  magic[4];
    uint16_t version;
    uint16_t offset;
    uint16_t load_addr;
    uint16_t init_addr;
    uint16_t play_addr;
    uint16_t song_count;
    uint16_t start_song;
    uint32_t speed;
    char     song_name[32];
    char     song_author[32];
    char     song_released[32];
    uint16_t flags;
    uint8_t  start_page;
    uint8_t  page_length;
    uint8_t  sid_addr_2;
    uint8_t  sid_addr_3;
};
#pragma pack(pop)


class Sid2Song {
public:

    bool run();

    const char* m_sid_filename = nullptr;
    const char* m_sng_filename = nullptr;
    bool        m_nopulse      = false;
    bool        m_nofilter     = false;
    bool        m_noinstrvib   = false;
    bool        m_fixedparams  = false;
    bool        m_nowavedelay  = false;

private:

    bool load_sid();

    uint8_t peek() {
        if (m_pos >= (int) m_data.size()) {
            printf("ERROR: peek\n");
            exit(1);
        }
        return m_data[m_pos];
    }
    uint8_t read() {
        if (m_pos >= (int) m_data.size()) {
            printf("ERROR: read\n");
            exit(1);
        }
        return m_data[m_pos++];
    }

    gt::Song             m_song = {};
    std::vector<uint8_t> m_data;
    int                  m_pos;
    int                  m_song_count;
    int                  m_addr_offset;
};


bool Sid2Song::load_sid() {
    std::ifstream ifs(m_sid_filename, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        printf("ERROR: could not open file\n");
        return false;
    }
    m_data.resize(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    ifs.read((char*) m_data.data(), m_data.size());


    SidHeader& h = *(SidHeader*) m_data.data();
    h.version    = swap(h.version);
    h.offset     = swap(h.offset);
    h.load_addr  = swap(h.load_addr);
    h.init_addr  = swap(h.init_addr);
    h.play_addr  = swap(h.play_addr);
    h.song_count = swap(h.song_count);
    h.start_song = swap(h.start_song);
    h.speed      = swap(h.speed);

    // ???
    h.load_addr = m_data[h.offset] | (m_data[h.offset + 1] << 8);

    printf("SID\n");
    printf(" magic:       %.4s\n", h.magic);
    printf(" version:     %d\n", h.version);
    printf(" offset:      %04X\n", h.offset);
    printf(" load addr:   %04X\n", h.load_addr);
    printf(" init addr:   %04X\n", h.init_addr);
    printf(" play addr:   %04X\n", h.play_addr);
    printf(" song count:  %d\n", h.song_count);
    printf(" start song:  %d\n", h.start_song);
    printf(" speed:       %08X\n", h.speed);
    printf(" song name:   %.32s\n", h.song_name);
    printf(" song author: %.32s\n", h.song_author);
    printf(" copyright:   %.32s\n", h.song_released);
    if (h.version > 1) {
        h.flags = swap(h.flags);
        printf(" flags:       %04X\n", h.flags);
        printf(" start page:  %d\n", h.start_page);
        printf(" page length: %d\n", h.page_length);
        printf(" sid 2 addr:  %d\n", h.sid_addr_2);
        printf(" sid 3 addr:  %d\n", h.sid_addr_3);
    }

    m_song.clear();
    memcpy(m_song.songname, h.song_name, sizeof(h.song_name));
    memcpy(m_song.authorname, h.song_author, sizeof(h.song_author));
    memcpy(m_song.copyrightname, h.song_released, sizeof(h.song_released));

    m_song_count  = h.song_count;
    m_addr_offset = h.offset - h.load_addr + 2;

    return true;
}


uint8_t const* find_mem(uint8_t const* haystack, int haystack_len, uint8_t const* needle, int needle_len) {
    for (uint8_t const* h = haystack; haystack_len >= needle_len; ++h, --haystack_len) {
        if (memcmp(h, needle, needle_len) == 0) return h;
    }
    return nullptr;
}


bool Sid2Song::run() {

    if (!load_sid()) return false;

    // find end of freq table
    uint8_t const FREQ_HI[] = "\x08\x09\x09\x0a\x0a\x0b\x0c\x0d\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                              "\x15\x17\x18\x1a\x1b\x1d\x1f\x20\x22\x24\x27\x29\x2b\x2e\x31\x34"
                              "\x37\x3a\x3e\x41\x45\x49\x4e\x52\x57\x5c\x62\x68\x6e\x75\x7c\x83"
                              "\x8b\x93\x9c\xa5\xaf\xb9\xc4\xd0\xdd\xea\xf8\xff";
    uint8_t const* hi = find_mem(m_data.data(), m_data.size(), FREQ_HI, 12);
    if (!hi) {
        printf("ERROR: no freq table\n");
        return false;
    }
    for (int i = 0; FREQ_HI[i] && *hi == FREQ_HI[i]; ++i) ++hi;
    m_pos = hi - m_data.data();

    // song table
    std::vector<int> song_order_list_pos(m_song_count);
    for (int& addr : song_order_list_pos) {
        addr = read();
        read();
        read();
    }
    for (int& addr : song_order_list_pos) {
        addr |= read() << 8;
        addr += m_addr_offset;
        read();
        read();
    }

    int patt_table_pos = m_pos;

    // song order list
    int patt_count = 0;
    for (int i = 0; i < m_song_count; ++i) {
        printf("SONG %d\n", i);

        m_pos = song_order_list_pos[i];

        for (int c = 0; c < 3; ++c) {
            printf(" %d:", c);
            int p = 0;
            for (;;) {
                int x = read();
                if (x == gt::LOOPSONG) break;
                if (x < gt::REPEAT) {
                    m_song.songorder[i][c][p++] = x;
                    patt_count = std::max(x + 1, patt_count);
                    printf(" %02X", x);
                }
                else if (x < gt::TRANSDOWN) {
                    // repeat
                    // swap with previous byte (i.e., pattern index)
                    m_song.songorder[i][c][p    ] = m_song.songorder[i][c][p - 1];
                    m_song.songorder[i][c][p - 1] = x;
                    ++p;
                    printf(" R%X", x - gt::REPEAT + 1);
                }
                else {
                    // transpose
                    m_song.songorder[i][c][p++] = x;
                    int q = x - gt::TRANSUP;
                    printf(" %c%X", "+-"[q < 0], abs(q));
                }
            }
            // pattern end
            int x = read();
            m_song.songorder[i][c][p++] = 0xff;
            m_song.songorder[i][c][p++] = x;
            printf(" RST%02X\n", x);
        }
    }


    // patterns
    int instr_count  = 0;
    int max_table[4] = {};

    for (int i = 0; m_pos < (int) m_data.size(); i++) {
        printf("PATTERN %02X\n", i);

        int prev_instr = 0;
        int instr      = 0;
        int cmd        = 0;
        int arg        = 0;
        int row_nr     = 0;

        for (;;) {
            prev_instr = instr;
            if (peek() < 0x40) {
                instr = read();
                instr_count = std::max(instr_count, instr);
            }

            int note;
            int repeat = 1;

            int x = read();
            if (x > gt::KEYON) {
                repeat = 256 - x;
                note = gt::REST;
            }
            else if (x >= gt::REST) {
                note = x;
            }
            else {
                if (x >= gt::FIRSTNOTE) {
                    note = x;
                }
                else {
                    cmd  = x % 16;
                    arg  = cmd ? read() : 0;
                    note = x < gt::FXONLY ? read() : gt::REST;

                    // inc tempo
                    if (cmd == 0xf && arg >=2) ++arg;

                    if ((cmd >= 0x1 && cmd <= 0x4) || cmd == 0xe) {
                        max_table[gt::STBL] = std::max(max_table[gt::STBL], arg);
                    }
                    if (cmd >= 0x8 && cmd <= 0xa) {
                        max_table[cmd - 0x8] = std::max(max_table[cmd - 0x8], arg);
                    }
                }
            }

            while (repeat--) {
                m_song.pattern[i][row_nr * 4 + 0] = note;
                m_song.pattern[i][row_nr * 4 + 1] = instr != prev_instr ? instr : 0;
                m_song.pattern[i][row_nr * 4 + 2] = cmd;
                m_song.pattern[i][row_nr * 4 + 3] = arg;

                printf(" %02X: ", row_nr++);
                if      (note == gt::REST)   printf("...");
                else if (note == gt::KEYOFF) printf("===");
                else if (note == gt::KEYON)  printf("+++");
                else printf("%c%c%d",
                            "CCDDEFFGGAAB"[note % 12],
                            "-#-#--#-#-#-"[note % 12],
                            (note - gt::FIRSTNOTE) / 12);
                printf(" %02X%X%02X\n", instr != prev_instr ? instr : 0, cmd, arg);
            }

            if (peek() == 0) break;
        }
        read();
        m_song.pattern[i][row_nr * 4] = gt::ENDPATT;
    }


    // skip pattern table
    m_pos = patt_table_pos + patt_count * 2;

    // instruments
    for (int i = 1; i <= instr_count; ++i) m_song.instr[i].ad = read();
    for (int i = 1; i <= instr_count; ++i) m_song.instr[i].sr = read();
    for (int i = 1; i <= instr_count; ++i) {
        int x = read();
        max_table[gt::WTBL] = std::max(max_table[gt::WTBL], x);
        m_song.instr[i].ptr[gt::WTBL] = x;
    }
    if (!m_nopulse) {
        for (int i = 1; i <= instr_count; ++i) {
            int x = read();
            max_table[gt::PTBL] = std::max(max_table[gt::PTBL], x);
            m_song.instr[i].ptr[gt::PTBL] = x;
        }
    }
    if (!m_nofilter) {
        for (int i = 1; i <= instr_count; ++i) {
            int x = read();
            max_table[gt::FTBL] = std::max(max_table[gt::FTBL], x);
            m_song.instr[i].ptr[gt::FTBL] = x;
        }
    }
    if (!m_noinstrvib) {
        for (int i = 1; i <= instr_count; ++i) {
            int x = read();
            max_table[gt::STBL] = std::max(max_table[gt::STBL], x);
            m_song.instr[i].ptr[gt::STBL] = x;
        }
        for (int i = 1; i <= instr_count; ++i) m_song.instr[i].vibdelay = read();
    }
    if (!m_fixedparams) {
        for (int i = 1; i <= instr_count; ++i) m_song.instr[i].gatetimer = read();
        for (int i = 1; i <= instr_count; ++i) m_song.instr[i].firstwave = read();
    }
    printf("INSTR\n");
    for (int i = 1; i <= instr_count; ++i) {
        auto const& instr = m_song.instr[i];
        printf(" %02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", i,
               instr.ad, instr.sr, instr.ptr[0], instr.ptr[1], instr.ptr[2], instr.ptr[3],
               instr.vibdelay, instr.gatetimer, instr.firstwave);
    }


    // tables
    for (int t = 0; t < gt::MAX_TABLES; ++t) {
        if (t == gt::PTBL && m_nopulse) continue;
        if (t == gt::FTBL && m_nofilter) continue;
        // TODO: maybe skip speed table
        printf("TABLE %d (min len %d)\n", t, max_table[t]);
        if (t == gt::STBL) {
            int x = read();
            if (x != 0) {
                printf("ERROR: speed table\n");
                return false;
            }
        }
        int x = 0;
        for (int i = 0; i < max_table[t]; ++i) {
            m_song.ltable[t][i] = x = read();
        }
        if (t < gt::STBL) {
            while (x != 0xff) {
                m_song.ltable[t][max_table[t]] = x = read();
                ++max_table[t];
            }
        }
        if (t == gt::STBL) {
            // keep reading until we find a zero
            while (peek() != 0) {
                m_song.ltable[t][max_table[t]] = read();
                ++max_table[t];
            }
            int x = read();
            if (x != 0) {
                printf("ERROR: speed table\n");
                return false;
            }
        }
        for (int i = 0; i < max_table[t]; ++i) {
            // read rtable
            m_song.rtable[t][i] = read();

            // fix stuff
            if (t == gt::WTBL) {
                if (!m_nowavedelay) {
                    int x = m_song.ltable[t][i];
                    if (x > 0x1f && x < 0xf0) x -= 0x10;
                    else if (x > 0x0f && x < 0x20) x += 0xd0;
                    m_song.ltable[t][i] = x;
                }

                // flip bit
                if (m_song.ltable[t][i] < gt::WAVECMD) m_song.rtable[t][i] ^= 0x80;
            }
            if (t == gt::FTBL) {
                int x = m_song.ltable[t][i];
                if (x > 0x80 && x < 0xff) {
                    m_song.ltable[t][i] = (x << 1) | 0x80;
                }
            }
            if (t == gt::STBL) {
                uint8_t x = m_song.ltable[t][i];
                if ((x >= 0xf1 && x <= 0xf4) || x == 0xfe) {
                    max_table[gt::STBL] = std::max<int>(max_table[gt::STBL], m_song.rtable[t][i]);
                }
            }

            printf(" %02X: %02X %02X\n", i + 1, m_song.ltable[t][i], m_song.rtable[t][i]);
        }
    }


    // sanity check
    if (m_pos > song_order_list_pos[0]) {
        printf("WARNING: read tables past order list (%d > %d)\n", m_pos, song_order_list_pos[0]);
    }
    if (m_pos < song_order_list_pos[0]) {
        printf("WARNING: not all table data was read (%d < %d)\n", m_pos, song_order_list_pos[0]);
    }

    std::string name = m_sng_filename ? m_sng_filename
                                      : std::filesystem::path(m_sid_filename).stem().string() + ".sng";
    return m_song.save(name.c_str());
}


int main(int argc, char** argv) {
    Sid2Song convert;
    int index = 0;
    for (int i = 1; i < argc; ++i) {
        char const* a = argv[i];
        if (a[0] != '-') {
            if      (index == 0) convert.m_sid_filename = a;
            else if (index == 1) convert.m_sng_filename = a;
            else if (index >= 2) goto USAGE;
            ++index;
            continue;
        }
        std::string s = a;
        if      (s == "-nopulse")     convert.m_nopulse     = true;
        else if (s == "-nofilter")    convert.m_nofilter    = true;
        else if (s == "-noinstrvib")  convert.m_noinstrvib  = true;
        else if (s == "-fixedparams") convert.m_fixedparams = true;
        else if (s == "-nowavedelay") convert.m_nowavedelay = true;
        else goto USAGE;
    }
    if (index == 0) goto USAGE;
    return convert.run() ? 0 : 1;

USAGE:
    fprintf(stderr, "usage: %s [options...] sid-file [sng-file]\n", argv[0]);
    fprintf(stderr, " -nopulse\n"
                    " -nofilter\n"
                    " -noinstrvib\n"
                    " -fixedparams\n"
                    " -nowavedelay\n");
    return 1;
}
