// =============================================================
// MAFIA: THE CITY OF LOST HEAVEN - DTA EXTRACTOR (STANDALONE)
// COMPLETE SOURCE - FIXED AUDIO ARITHMETIC (UINT16 WRAP)
// =============================================================

#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <stdio.h>
#include <cstdint>

#pragma comment(lib, "Comctl32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// -------------------------------------------------------------------------
// CONSTANTS & GLOBALS
// -------------------------------------------------------------------------
#define ID_BTN_EXTRACT      102
#define ID_LISTBOX          103
#define ID_PROGRESS         104
#define ID_STATUS           105
#define ID_BTN_SELECTALL    106
#define ID_BTN_PATCH        107
#define ID_LBL_CREDITS      108
#define ID_RPT_EDIT         201
#define ID_RPT_EXIT         202

HWND hList, hProgress, hStatus, hBtnExtract, hBtnSelectAll, hBtnPatch, hLblCredits;
HWND hMainWindow;
HINSTANCE hInst;
bool g_bAbortExtraction = false;

// -------------------------------------------------------------------------
// DTA STRUCTURES
// -------------------------------------------------------------------------
#pragma pack(push, 1)
struct DTAHeader {
    uint32_t FileCount;
    uint32_t OffsetToContentTable;
    uint32_t SizeOfContentTable;
    uint32_t Unknown;
};

struct ContentTableEntry {
    uint16_t FileNameChecksum;
    uint16_t FileNameLength;
    uint32_t OffsetToFileInfoHeader; // mHeaderOffset
    uint32_t OffsetToData;           // mDataOffset
    char     FilenameHint[16];
};

struct FileInfoHeader {
    uint32_t Unknown1;
    uint32_t Unknown2;
    uint64_t TimeStamp;
    uint32_t FileSize;
    uint32_t BlockCount;
    uint8_t  NameLength;
    uint8_t  Flags[7];
};

struct WavHeader {
    char riff[4];
    uint32_t size;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t format;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
};
#pragma pack(pop)

enum BlockType {
    BLOCK_UNCOMPRESSED = 0,
    BLOCK_LZSS_RLE = 1,
    BLOCK_DPCM0 = 8,
    BLOCK_DPCM1 = 12,
    BLOCK_DPCM2 = 16,
    BLOCK_DPCM3 = 20,
    BLOCK_DPCM4 = 24,
    BLOCK_DPCM5 = 28,
    BLOCK_DPCM6 = 32
};

struct DtaInfo {
    uint32_t k1;
    uint32_t k2;
    const char* description;
};

// Full DPCM Look-up Table (Matches parser_dta.txt)
static uint16_t WAV_DELTAS[] =
{
    0x0000,0x0001,0x0002,0x0004,0x0008,0x000c,0x0012,0x0018,0x0020,0x0029,0x0032,0x003d,
    0x0049,0x0055,0x0063,0x0072,0x0082,0x0092,0x00a4,0x00b7,0x00cb,0x00df,0x00f5,0x010c,
    0x0124,0x013d,0x0157,0x0172,0x018e,0x01ab,0x01c9,0x01e8,0x0208,0x0229,0x024b,0x026e,
    0x0292,0x02b7,0x02dd,0x0304,0x032c,0x0355,0x037f,0x03ab,0x03d7,0x0404,0x0432,0x0461,
    0x0492,0x04c3,0x04f5,0x0529,0x055d,0x0592,0x05c9,0x0600,0x0638,0x0672,0x06ac,0x06e8,
    0x0724,0x0761,0x07a0,0x07df,0x0820,0x0861,0x08a4,0x08e7,0x092c,0x0972,0x09b8,0x0a00,
    0x0a48,0x0a92,0x0add,0x0b28,0x0b75,0x0bc3,0x0c12,0x0c61,0x0cb2,0x0d04,0x0d57,0x0daa,
    0x0dff,0x0e55,0x0eac,0x0f04,0x0f5d,0x0fb7,0x1012,0x106d,0x10ca,0x1128,0x1187,0x11e7,
    0x1248,0x12aa,0x130d,0x1371,0x13d7,0x143d,0x14a4,0x150c,0x1575,0x15df,0x164a,0x16b7,
    0x1724,0x1792,0x1801,0x1871,0x18e3,0x1955,0x19c8,0x1a3d,0x1ab2,0x1b28,0x1ba0,0x1c18,
    0x1c91,0x1d0c,0x1d87,0x1e04,0x1e81,0x1f00,0x1f7f,0x2000,0x0000,0x0001,0x0003,0x0006,
    0x000c,0x0013,0x001b,0x0025,0x0030,0x003d,0x004c,0x005c,0x006d,0x0080,0x0095,0x00ab,
    0x00c3,0x00dc,0x00f6,0x0113,0x0130,0x014f,0x0170,0x0193,0x01b6,0x01dc,0x0203,0x022b,
    0x0255,0x0280,0x02ad,0x02dc,0x030c,0x033d,0x0370,0x03a5,0x03db,0x0412,0x044c,0x0486,
    0x04c2,0x0500,0x053f,0x0580,0x05c2,0x0606,0x064c,0x0692,0x06db,0x0725,0x0770,0x07bd,
    0x080c,0x085c,0x08ad,0x0900,0x0955,0x09ab,0x0a02,0x0a5c,0x0ab6,0x0b12,0x0b70,0x0bcf,
    0x0c30,0x0c92,0x0cf6,0x0d5b,0x0dc2,0x0e2b,0x0e95,0x0f00,0x0f6d,0x0fdb,0x104b,0x10bd,
    0x1130,0x11a5,0x121b,0x1292,0x130b,0x1386,0x1402,0x1480,0x14ff,0x1580,0x1602,0x1686,
    0x170b,0x1792,0x181b,0x18a4,0x1930,0x19bd,0x1a4b,0x1adb,0x1b6d,0x1c00,0x1c94,0x1d2a,
    0x1dc2,0x1e5b,0x1ef6,0x1f92,0x2030,0x20cf,0x2170,0x2212,0x22b6,0x235b,0x2402,0x24aa,
    0x2554,0x2600,0x26ad,0x275b,0x280b,0x28bd,0x2970,0x2a24,0x2ada,0x2b92,0x2c4b,0x2d06,
    0x2dc2,0x2e80,0x2f3f,0x3000,0x0000,0x0002,0x0004,0x0009,0x0010,0x0019,0x0024,0x0031,
    0x0041,0x0052,0x0065,0x007a,0x0092,0x00ab,0x00c7,0x00e4,0x0104,0x0125,0x0149,0x016e,
    0x0196,0x01bf,0x01eb,0x0219,0x0249,0x027a,0x02ae,0x02e4,0x031c,0x0356,0x0392,0x03d0,
    0x0410,0x0452,0x0496,0x04dc,0x0524,0x056e,0x05ba,0x0609,0x0659,0x06ab,0x06ff,0x0756,
    0x07ae,0x0809,0x0865,0x08c3,0x0924,0x0986,0x09eb,0x0a52,0x0aba,0x0b25,0x0b92,0x0c00,
    0x0c71,0x0ce4,0x0d59,0x0dd0,0x0e48,0x0ec3,0x0f40,0x0fbf,0x1040,0x10c3,0x1148,0x11cf,
    0x1259,0x12e4,0x1371,0x1400,0x1491,0x1525,0x15ba,0x1651,0x16eb,0x1786,0x1824,0x18c3,
    0x1965,0x1a08,0x1aae,0x1b55,0x1bff,0x1cab,0x1d58,0x1e08,0x1eba,0x1f6e,0x2024,0x20db,
    0x2195,0x2251,0x230f,0x23cf,0x2491,0x2555,0x261b,0x26e3,0x27ae,0x287a,0x2948,0x2a18,
    0x2aeb,0x2bbf,0x2c95,0x2d6e,0x2e48,0x2f24,0x3003,0x30e3,0x31c6,0x32aa,0x3391,0x347a,
    0x3564,0x3651,0x3740,0x3830,0x3923,0x3a18,0x3b0f,0x3c08,0x3d03,0x3e00,0x3eff,0x4000,
    0x0000,0x0002,0x0005,0x000b,0x0014,0x001f,0x002d,0x003e,0x0051,0x0066,0x007e,0x0099,
    0x00b6,0x00d6,0x00f8,0x011d,0x0145,0x016e,0x019b,0x01ca,0x01fb,0x022f,0x0266,0x029f,
    0x02db,0x0319,0x035a,0x039d,0x03e3,0x042b,0x0476,0x04c4,0x0514,0x0566,0x05bb,0x0613,
    0x066d,0x06ca,0x0729,0x078b,0x07ef,0x0856,0x08bf,0x092b,0x099a,0x0a0b,0x0a7e,0x0af4,
    0x0b6d,0x0be8,0x0c66,0x0ce6,0x0d69,0x0dee,0x0e76,0x0f01,0x0f8d,0x101d,0x10af,0x1144,
    0x11db,0x1274,0x1310,0x13af,0x1450,0x14f4,0x159b,0x1643,0x16ef,0x179d,0x184d,0x1900,
    0x19b6,0x1a6e,0x1b29,0x1be6,0x1ca6,0x1d68,0x1e2d,0x1ef4,0x1fbe,0x208a,0x2159,0x222b,
    0x22ff,0x23d6,0x24af,0x258a,0x2669,0x2749,0x282d,0x2912,0x29fb,0x2ae6,0x2bd3,0x2cc3,
    0x2db6,0x2eab,0x2fa2,0x309c,0x3199,0x3298,0x339a,0x349e,0x35a5,0x36af,0x37bb,0x38c9,
    0x39da,0x3aee,0x3c04,0x3d1c,0x3e37,0x3f55,0x4075,0x4198,0x42bd,0x43e5,0x4510,0x463d,
    0x476c,0x489e,0x49d3,0x4b0a,0x4c43,0x4d80,0x4ebe,0x5000,0x0000,0x0002,0x0006,0x000d,
    0x0018,0x0026,0x0036,0x004a,0x0061,0x007b,0x0098,0x00b8,0x00db,0x0101,0x012a,0x0156,
    0x0186,0x01b8,0x01ed,0x0226,0x0261,0x029f,0x02e1,0x0326,0x036d,0x03b8,0x0406,0x0456,
    0x04aa,0x0501,0x055b,0x05b8,0x0618,0x067b,0x06e1,0x074a,0x07b6,0x0825,0x0898,0x090d,
    0x0985,0x0a01,0x0a7f,0x0b01,0x0b85,0x0c0d,0x0c98,0x0d25,0x0db6,0x0e4a,0x0ee1,0x0f7b,
    0x1018,0x10b8,0x115b,0x1201,0x12aa,0x1356,0x1405,0x14b8,0x156d,0x1625,0x16e1,0x179f,
    0x1861,0x1925,0x19ed,0x1ab7,0x1b85,0x1c56,0x1d2a,0x1e01,0x1eda,0x1fb7,0x2097,0x217a,
    0x2260,0x234a,0x2436,0x2525,0x2617,0x270d,0x2805,0x2900,0x29ff,0x2b00,0x2c05,0x2d0c,
    0x2e17,0x2f25,0x3036,0x3149,0x3260,0x337a,0x3497,0x35b7,0x36da,0x3800,0x3929,0x3a55,
    0x3b85,0x3cb7,0x3dec,0x3f25,0x4060,0x419e,0x42e0,0x4425,0x456c,0x46b7,0x4804,0x4955,
    0x4aa9,0x4c00,0x4d5a,0x4eb7,0x5017,0x517a,0x52e0,0x5449,0x55b5,0x5724,0x5896,0x5a0c,
    0x5b84,0x5d00,0x5e7e,0x6000,0x0000,0x0002,0x0007,0x000f,0x001c,0x002c,0x003f,0x0057,
    0x0071,0x008f,0x00b1,0x00d7,0x00ff,0x012c,0x015c,0x018f,0x01c7,0x0201,0x023f,0x0281,
    0x02c7,0x030f,0x035c,0x03ac,0x03ff,0x0457,0x04b1,0x050f,0x0571,0x05d7,0x063f,0x06ac,
    0x071c,0x078f,0x0806,0x0881,0x08ff,0x0981,0x0a06,0x0a8f,0x0b1c,0x0bac,0x0c3f,0x0cd6,
    0x0d71,0x0e0f,0x0eb1,0x0f56,0x0fff,0x10ac,0x115c,0x120f,0x12c6,0x1381,0x143f,0x1501,
    0x15c6,0x168f,0x175c,0x182c,0x18ff,0x19d6,0x1ab1,0x1b8f,0x1c71,0x1d56,0x1e3f,0x1f2b,
    0x201b,0x210f,0x2206,0x2301,0x23ff,0x2501,0x2606,0x270f,0x281b,0x292b,0x2a3f,0x2b56,
    0x2c71,0x2d8f,0x2eb1,0x2fd6,0x30ff,0x322b,0x335b,0x348f,0x35c6,0x3700,0x383f,0x3980,
    0x3ac6,0x3c0f,0x3d5b,0x3eab,0x3ffe,0x4156,0x42b0,0x440e,0x4570,0x46d5,0x483e,0x49ab,
    0x4b1b,0x4c8e,0x4e05,0x4f80,0x50fe,0x5280,0x5405,0x558e,0x571b,0x58ab,0x5a3e,0x5bd5,
    0x5d70,0x5f0e,0x60b0,0x6255,0x63fe,0x65aa,0x675a,0x690e,0x6ac5,0x6c80,0x6e3e,0x7000,
    0x0000,0x0002,0x0008,0x0012,0x0020,0x0032,0x0049,0x0063,0x0082,0x00a4,0x00cb,0x00f5,
    0x0124,0x0157,0x018e,0x01c9,0x0208,0x024b,0x0292,0x02dd,0x032c,0x037f,0x03d7,0x0432,
    0x0492,0x04f5,0x055d,0x05c9,0x0638,0x06ac,0x0724,0x07a0,0x0820,0x08a4,0x092c,0x09b8,
    0x0a48,0x0add,0x0b75,0x0c11,0x0cb2,0x0d57,0x0dff,0x0eac,0x0f5d,0x1011,0x10ca,0x1187,
    0x1248,0x130d,0x13d6,0x14a4,0x1575,0x164a,0x1724,0x1801,0x18e2,0x19c8,0x1ab2,0x1b9f,
    0x1c91,0x1d87,0x1e81,0x1f7f,0x2081,0x2187,0x2291,0x239f,0x24b1,0x25c8,0x26e2,0x2801,
    0x2923,0x2a4a,0x2b74,0x2ca3,0x2dd6,0x2f0d,0x3047,0x3186,0x32c9,0x3411,0x355c,0x36ab,
    0x37fe,0x3956,0x3ab1,0x3c10,0x3d74,0x3edb,0x4047,0x41b7,0x432b,0x44a2,0x461e,0x479e,
    0x4922,0x4aaa,0x4c37,0x4dc7,0x4f5b,0x50f3,0x5290,0x5430,0x55d5,0x577d,0x592a,0x5adb,
    0x5c90,0x5e48,0x6005,0x61c6,0x638b,0x6554,0x6722,0x68f3,0x6ac8,0x6ca1,0x6e7f,0x7060,
    0x7246,0x7430,0x761d,0x780f,0x7a05,0x7bff,0x7dfd,0x7fff
};

const DtaInfo DEFAULT_DTA_INFO = { 0xA1B2C3D4, 0x23463458, "Unknown DTA (Defaults)" };

std::map<std::string, DtaInfo> DTA_MAP = {
    {"A0.dta", {0xD8D0A975, 0x467ACDE0, "Sounds"}},
    {"A1.dta", {0x3D98766C, 0xDE7009CD, "Missions"}},
    {"A2.dta", {0x82A1C97B, 0x2D5085D4, "Models"}},
    {"A3.dta", {0x43876FEA, 0x900CDBA8, "Animations I"}},
    {"A4.dta", {0x43876FEA, 0x900CDBA8, "Animations II"}},
    {"A5.dta", {0xDEAC5342, 0x760CE652, "Diff Data"}},
    {"A6.dta", {0x64CD8D0A, 0x4BC97B2D, "Textures"}},
    {"A7.dta", {0xD6FEA900, 0xCDB76CE6, "Records"}},
    {"a8.dta", {0xD8DD8FAC, 0x5324ACE5, "Patch Files"}},
    {"A9.dta", {0x6FEE6324, 0xACDA4783, "System"}},
    {"AA.dta", {0x5342760C, 0xEDEAC652, "Tables"}},
    {"AB.dta", {0xD8D0A975, 0x467ACDE0, "Music"}},
    {"AC.dta", {0x43876FEA, 0x900CDBA8, "Animations III"}},
    {"ISdata.dta", {0xA1B2C3D4, 0x23463458, "Special Data"}},
    {"bilboard0.dta", {0xA1B2C3D4, 0x23463458, "Installation Images I"}},
    {"bilboard1.dta", {0xA1B2C3D4, 0x23463458, "Installation Images II"}},
    {"bilboard2.dta", {0xA1B2C3D4, 0x23463458, "Installation Images III"}}
};

// -------------------------------------------------------------------------
// CORE LOGIC
// -------------------------------------------------------------------------

void DecryptBlock(void* pBuffer, size_t size, uint32_t k1, uint32_t k2) {
    size_t cLongLong = size / 8;
    uint32_t* pLong = (uint32_t*)pBuffer;
    while (cLongLong--) {
        uint32_t ulong = pLong[0];
        pLong[0] = ~((~ulong) ^ k2);
        ulong = pLong[1];
        pLong[1] = ~((~ulong) ^ k1);
        pLong += 2;
    }
    size_t processedBytes = (size / 8) * 8;
    uint8_t* pByte = ((uint8_t*)pBuffer) + processedBytes;
    size_t cBytes = size % 8;
    uint32_t keys[2] = { k2, k1 };
    uint8_t* pKeyBytes = (uint8_t*)keys;
    for (size_t i = 0; i < cBytes; ++i) {
        uint8_t b = pByte[i];
        uint8_t k = pKeyBytes[i];
        pByte[i] = (uint8_t)(~((~b) ^ k));
    }
}

void DecompressLZSS(const std::vector<char>& input, std::vector<char>& output) {
    size_t pos = 0;
    size_t len = input.size();
    const unsigned char* buf = (const unsigned char*)input.data();
    while (pos < len) {
        if (pos + 2 > len) break;
        uint16_t flags = (buf[pos] << 8) | (buf[pos + 1]);
        pos += 2;
        if (flags == 0) {
            size_t n = 16;
            if (n > len - pos) n = len - pos;
            output.insert(output.end(), buf + pos, buf + pos + n);
            pos += n;
        }
        else {
            for (int i = 0; i < 16 && pos < len; ++i) {
                if (flags & 0x8000) {
                    if (pos + 2 > len) break;
                    uint32_t b1 = buf[pos];
                    uint32_t b2 = buf[pos + 1];
                    uint32_t offset = (b1 << 4) | (b2 >> 4);
                    uint32_t n = (b2 & 0x0F);
                    if (offset == 0) {
                        if (pos + 4 > len) break;
                        n = ((n << 8) | buf[pos + 2]) + 16;
                        char val = (char)buf[pos + 3];
                        output.insert(output.end(), n, val);
                        pos += 4;
                    }
                    else {
                        n += 3;
                        size_t currentSize = output.size();
                        if (offset > currentSize) {
                            output.insert(output.end(), n, 0x20);
                        }
                        else {
                            for (size_t j = 0; j < n; ++j) {
                                output.push_back(output[currentSize - offset + j]);
                            }
                        }
                        pos += 2;
                    }
                }
                else {
                    output.push_back(buf[pos++]);
                }
                flags <<= 1;
            }
        }
    }
}

// -------------------------------------------------------------------------
// DPCM AUDIO LOGIC
// -------------------------------------------------------------------------

void DecompressDPCM(
    int dpcmType,
    uint8_t* buffer,
    size_t bufferLen,
    std::vector<char>& out,
    WavHeader& wavHeader,
    uint32_t k1,
    uint32_t k2
) {
    // Safety check for start value
    if (bufferLen < 2) return;

    // Use uint16_t to match reference behavior (Implicit Overflow Wrapping)
    uint16_t value = (uint8_t)buffer[0] | ((uint8_t)buffer[1] << 8);

    // Output as standard 16-bit LE (just raw bytes of the uint16)
    out.push_back((char)(value & 0xFF));
    out.push_back((char)(value >> 8));

    size_t pos = 2;
    uint16_t* delta = &WAV_DELTAS[128 * dpcmType];

    if (wavHeader.channels == 1) {
        while (pos < bufferLen) {
            // Ref: sign = (b & 0x80) == 0 ? 1.0 : -1.0;
            // Matches: if bit set -> -1, if clear -> 1
            int sign = (buffer[pos] & 0x80) ? -1 : 1;

            // Arithmetic on unsigned (wraps on overflow/underflow)
            value += sign * delta[buffer[pos] & 0x7F];

            out.push_back((char)(value & 0xFF));
            out.push_back((char)(value >> 8));
            pos++;
        }
    }
    else {
        // Fallback for Stereo: Copy raw
        while (pos < bufferLen) {
            out.push_back(buffer[pos]);
            pos++;
        }
    }
}

// -------------------------------------------------------------------------
// HELPERS
// -------------------------------------------------------------------------

void Log(const char* fmt, ...) {
    if (!IsWindow(hStatus)) return;
    char buffer[1024];
    va_list args; va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    SetWindowTextA(hStatus, buffer);
    RECT rect;
    GetWindowRect(hStatus, &rect);
    ScreenToClient(hMainWindow, (LPPOINT)&rect);
    ScreenToClient(hMainWindow, ((LPPOINT)&rect) + 1);
    InvalidateRect(hMainWindow, &rect, TRUE);
    UpdateWindow(hMainWindow);
}

bool CheckProcessMessages() {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_bAbortExtraction = true;
            PostQuitMessage((int)msg.wParam);
            return false;
        }
        TranslateMessage(&msg); DispatchMessageA(&msg);
    }
    return true;
}

std::string ToUpper(const std::string& s) {
    std::string ret = s;
    for (size_t i = 0; i < ret.length(); ++i) ret[i] = (char)toupper((unsigned char)ret[i]);
    return ret;
}

void CreatePathRecursively(const std::string& path) {
    if (path.empty()) return;
    char buffer[MAX_PATH];
    strcpy_s(buffer, MAX_PATH, path.c_str());
    for (char* p = buffer; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char temp = *p; *p = 0;
            if (strlen(buffer) > 0) CreateDirectoryA(buffer, NULL);
            *p = temp;
        }
    }
}

// -------------------------------------------------------------------------
// PATCHING BUTTON LOGIC
// -------------------------------------------------------------------------

void UpdatePatchButtonState() {
    if (!IsWindow(hBtnPatch)) return;
    std::ifstream dll("rw_data.dll", std::ios::binary);
    if (!dll.is_open()) {
        SetWindowTextA(hBtnPatch, "rw_data.dll not found");
        EnableWindow(hBtnPatch, FALSE);
        return;
    }
    dll.seekg(0x4720, std::ios::beg);
    unsigned char byteVal = 0;
    dll.read((char*)&byteVal, 1);
    dll.close();
    EnableWindow(hBtnPatch, TRUE);
    if (byteVal == 0xC6) SetWindowTextA(hBtnPatch, "Patch rw_data.dll");
    else if (byteVal == 0xC3) SetWindowTextA(hBtnPatch, "Restore rw_data.dll");
    else { SetWindowTextA(hBtnPatch, "Unknown Version"); EnableWindow(hBtnPatch, FALSE); }
}

void TogglePatchRwData() {
    std::fstream dll("rw_data.dll", std::ios::binary | std::ios::in | std::ios::out);
    if (!dll.is_open()) { MessageBoxA(hMainWindow, "rw_data.dll not found.", "Error", MB_ICONERROR); return; }
    dll.seekg(0x4720, std::ios::beg);
    unsigned char byteVal = 0;
    dll.read((char*)&byteVal, 1);
    if (byteVal == 0xC6) {
        dll.seekp(0x4720, std::ios::beg); unsigned char patch = 0xC3; dll.write((char*)&patch, 1);
        MessageBoxA(hMainWindow, "rw_data.dll patched.", "Success", MB_ICONINFORMATION);
    }
    else if (byteVal == 0xC3) {
        dll.seekp(0x4720, std::ios::beg); unsigned char original = 0xC6; dll.write((char*)&original, 1);
        MessageBoxA(hMainWindow, "rw_data.dll restored.", "Success", MB_ICONINFORMATION);
    }
    dll.close();
    UpdatePatchButtonState();
}

// -------------------------------------------------------------------------
// UI WINDOWS
// -------------------------------------------------------------------------

void ScanCurrentDirectory() {
    SendMessageA(hList, LB_RESETCONTENT, 0, 0);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA("*.dta", &ffd);
    if (hFind == INVALID_HANDLE_VALUE) { Log("No .dta files found."); return; }
    int count = 0;
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string filename = ffd.cFileName;
            std::string upperName = ToUpper(filename);
            std::string displayString = filename;

            bool found = false;
            for (auto it = DTA_MAP.begin(); it != DTA_MAP.end(); ++it) {
                if (ToUpper(it->first) == upperName) {
                    displayString += " - ";
                    displayString += it->second.description;
                    found = true;
                    break;
                }
            }
            if (!found) displayString += " - (Unknown DTA)";
            SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)displayString.c_str());
            count++;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
    Log("Found %d DTA files.", count);
}

void ToggleSelectAll() {
    int count = (int)SendMessageA(hList, LB_GETCOUNT, 0, 0);
    int selCount = (int)SendMessageA(hList, LB_GETSELCOUNT, 0, 0);
    if (count == 0) return;
    BOOL selectAction = (selCount < count) ? TRUE : FALSE;
    SendMessageA(hList, LB_SETSEL, (WPARAM)selectAction, (LPARAM)-1);
}

LRESULT CALLBACK ReportWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Consolas");
        HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY, 10, 10, 360, 200, hwnd, (HMENU)ID_RPT_EDIT, hInst, NULL);
        SendMessageA(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND hBtnExit = CreateWindowA("BUTTON", "Exit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 130, 220, 120, 30, hwnd, (HMENU)ID_RPT_EXIT, hInst, NULL);
        SendMessageA(hBtnExit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    } break;
    case WM_COMMAND: if (LOWORD(wParam) == ID_RPT_EXIT) DestroyWindow(hwnd); break;
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowReportWindow(const std::string& report) {
    if (g_bAbortExtraction) return;
    WNDCLASSA wc = { 0 }; wc.lpfnWndProc = ReportWndProc; wc.hInstance = hInst; wc.lpszClassName = "MafiaReportWnd"; wc.hCursor = LoadCursorA(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);
    RECT rc; GetWindowRect(hMainWindow, &rc);
    HWND hRpt = CreateWindowExA(WS_EX_DLGMODALFRAME, "MafiaReportWnd", "Extraction Complete", WS_VISIBLE | WS_SYSMENU | WS_CAPTION, rc.left + 20, rc.top + 20, 400, 300, hMainWindow, NULL, hInst, NULL);
    SetDlgItemTextA(hRpt, ID_RPT_EDIT, report.c_str());
    EnableWindow(hMainWindow, FALSE);
    MSG msg = { 0 }; while (GetMessageA(&msg, NULL, 0, 0)) { if (!IsDialogMessageA(hRpt, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); } if (!IsWindow(hRpt)) break; }
    EnableWindow(hMainWindow, TRUE);
}

// -------------------------------------------------------------------------
// EXTRACTION LOGIC
// -------------------------------------------------------------------------

void ProcessExtraction() {
    g_bAbortExtraction = false;
    int count = (int)SendMessageA(hList, LB_GETSELCOUNT, 0, 0);
    if (count <= 0) { MessageBoxA(NULL, "Please select at least one file.", "Info", MB_OK); return; }

    EnableWindow(hBtnExtract, FALSE); EnableWindow(hBtnSelectAll, FALSE); EnableWindow(hBtnPatch, FALSE);

    int* selectedIndices = new int[count];
    SendMessageA(hList, LB_GETSELITEMS, (WPARAM)count, (LPARAM)selectedIndices);
    std::vector<std::string> filesToProcess;
    for (int i = 0; i < count; ++i) {
        char buf[256] = { 0 };
        SendMessageA(hList, LB_GETTEXT, selectedIndices[i], (LPARAM)buf);
        std::string s = buf;
        size_t dash = s.find(" - ");
        filesToProcess.push_back(dash != std::string::npos ? s.substr(0, dash) : s);
    }
    delete[] selectedIndices;

    auto itA8 = std::find_if(filesToProcess.begin(), filesToProcess.end(), [](const std::string& f) {
        std::string upper = f;
        for (auto& c : upper) c = (char)toupper((unsigned char)c);
        return upper == "A8.DTA"; // NESAHAT
        });

    if (itA8 != filesToProcess.end()) {
        std::string a8Path = *itA8;
        filesToProcess.erase(itA8);
        filesToProcess.push_back(a8Path);
    }

    std::string report = "Extraction Report:\r\n==================\r\n";

    for (const auto& filename : filesToProcess) {
        if (g_bAbortExtraction) break;

        uint32_t k1, k2;
        auto itMap = DTA_MAP.find(filename);

        if (itMap != DTA_MAP.end()) {
            Log("Processing %s...", filename.c_str());
            k1 = itMap->second.k1;
            k2 = itMap->second.k2;
        }
        else {
            Log("Processing %s using default keys...", filename.c_str());
            k1 = DEFAULT_DTA_INFO.k1;
            k2 = DEFAULT_DTA_INFO.k2;
        }

        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) { report += filename + ": FAILED (File not found)\r\n"; continue; }

        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        char magic[5] = { 0 };
        file.read(magic, 4);
        if (strcmp(magic, "ISD0") != 0) { report += filename + ": FAILED (Invalid Magic)\r\n"; continue; }

        DTAHeader header;
        file.read((char*)&header, sizeof(header));
        DecryptBlock(&header, sizeof(header), k1, k2);

        if (header.FileCount > 100000 || header.OffsetToContentTable > (uint32_t)fileSize) {
            report += filename + ": FAILED (Decryption failed/Bad Keys)\r\n";
            continue;
        }

        std::vector<ContentTableEntry> table(header.FileCount);
        file.seekg(header.OffsetToContentTable, std::ios::beg);
        file.read((char*)table.data(), header.FileCount * sizeof(ContentTableEntry));
        DecryptBlock(table.data(), table.size() * sizeof(ContentTableEntry), k1, k2);

        SendMessageA(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, header.FileCount));
        int extracted = 0;

        for (uint32_t f = 0; f < header.FileCount; ++f) {
            if (f % 20 == 0 && !CheckProcessMessages()) break;

            file.seekg(table[f].OffsetToFileInfoHeader, std::ios::beg);
            FileInfoHeader fh;
            file.read((char*)&fh, sizeof(fh));
            DecryptBlock(&fh, sizeof(fh), k1, k2);

            if (fh.NameLength == 0 || fh.NameLength > 255) continue;

            std::vector<char> nameBuf(fh.NameLength + 1, 0);
            file.read(nameBuf.data(), fh.NameLength);
            DecryptBlock(nameBuf.data(), fh.NameLength, k1, k2);
            std::string internalName = nameBuf.data();

            // USE DATA OFFSET FROM TABLE
            uint32_t dataStart = table[f].OffsetToData;
            file.seekg(dataStart, std::ios::beg);

            std::vector<char> finalData;
            finalData.reserve(fh.FileSize);

            // Persistent state for current file
            bool wavHeaderRead = false;
            WavHeader wavHeader{};

            for (uint32_t b = 0; b < fh.BlockCount; ++b) {
                uint32_t bSz;
                file.read((char*)&bSz, 4);
                bSz &= 0xFFFF;

                if (bSz == 0) continue;

                std::vector<char> blk(bSz);
                file.read(blk.data(), bSz);

                // Check Encryption Flag: mFlags[0] is bit 0x80
                if (fh.Flags[0] & 0x80) DecryptBlock(blk.data(), bSz, k1, k2);

                uint8_t blockType = (uint8_t)blk[0];

                if (blockType == BLOCK_UNCOMPRESSED) {
                    finalData.insert(finalData.end(), blk.begin() + 1, blk.end());

                    // If file started with a raw block (not DPCM), check if it was a header
                    if (!wavHeaderRead && finalData.size() >= sizeof(WavHeader)) {
                        memcpy(&wavHeader, finalData.data(), sizeof(WavHeader));
                        if (memcmp(wavHeader.riff, "RIFF", 4) == 0) {
                            wavHeaderRead = true;
                        }
                    }
                }
                else if (blockType == BLOCK_LZSS_RLE) {
                    std::vector<char> tmp;
                    DecompressLZSS(
                        std::vector<char>(blk.begin() + 1, blk.end()),
                        tmp
                    );
                    finalData.insert(finalData.end(), tmp.begin(), tmp.end());
                }
                // Updated Logic: Check range 8 to 32
                else if (blockType >= BLOCK_DPCM0 && blockType <= BLOCK_DPCM6) {
                    int dpcmType = (blockType - 8) / 4;

                    // Header Decryption Logic
                    if (!wavHeaderRead) {
                        if (blk.size() > 1 + sizeof(WavHeader)) {
                            memcpy(&wavHeader, blk.data() + 1, sizeof(WavHeader));
                            // DOUBLE DECRYPT HEADER
                            DecryptBlock(&wavHeader, sizeof(WavHeader), k1, k2);

                            // Validate
                            if (memcmp(wavHeader.riff, "RIFF", 4) != 0) {
                                // Fallback Synth
                                memset(&wavHeader, 0, sizeof(WavHeader));
                                memcpy(wavHeader.riff, "RIFF", 4);
                                memcpy(wavHeader.wave, "WAVE", 4);
                                memcpy(wavHeader.fmt, "fmt ", 4);
                                wavHeader.fmtSize = 16;
                                wavHeader.format = 1;
                                wavHeader.channels = 1;
                                wavHeader.sampleRate = 44100;
                                wavHeader.bitsPerSample = 16;
                                wavHeader.blockAlign = 2;
                                wavHeader.byteRate = 44100 * 2;
                                memcpy(wavHeader.data, "data", 4);
                            }

                            wavHeaderRead = true;
                            finalData.insert(finalData.end(), (char*)&wavHeader, (char*)&wavHeader + sizeof(WavHeader));

                            // Pass REST of data to decompressor (Skip Type + Header)
                            DecompressDPCM(
                                dpcmType,
                                (uint8_t*)blk.data() + 1 + sizeof(WavHeader),
                                blk.size() - 1 - sizeof(WavHeader),
                                finalData,
                                wavHeader,
                                k1, k2
                            );
                        }
                    }
                    else {
                        // Header already read, just decompress data (Skip Type)
                        DecompressDPCM(
                            dpcmType,
                            (uint8_t*)blk.data() + 1,
                            blk.size() - 1,
                            finalData,
                            wavHeader,
                            k1, k2
                        );
                    }
                }
                else {
                    // Unknown blocks
                    if (blk.size() > 1) {
                        finalData.insert(finalData.end(), blk.begin() + 1, blk.end());
                    }
                }
            }

            CreatePathRecursively(internalName);
            std::ofstream out(internalName, std::ios::binary);
            if (out.is_open()) {
                out.write(finalData.data(), finalData.size());
                extracted++;
            }
            SendMessageA(hProgress, PBM_SETPOS, f, 0);
        }

        char sum[128];
        const char* statusPrefix = (itMap == DTA_MAP.end()) ? ": SUCCESS (Used Defaults - " : ": SUCCESS (";
        sprintf_s(sum, 128, "%s%d files)\r\n", statusPrefix, extracted);
        report += filename + sum;
    }

    EnableWindow(hBtnExtract, TRUE); EnableWindow(hBtnSelectAll, TRUE); EnableWindow(hBtnPatch, TRUE);
    if (!g_bAbortExtraction) ShowReportWindow(report);
}

// -------------------------------------------------------------------------
// WNDPROC / MAIN
// -------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        hMainWindow = hwnd;
        HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HFONT hFontSmall = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

        HWND hLabel = CreateWindowA("STATIC", "Select DTA files for extraction:", WS_CHILD | WS_VISIBLE | SS_SIMPLE, 10, 10, 300, 20, hwnd, (HMENU)999, hInst, NULL);
        SendMessageA(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_MULTIPLESEL, 10, 35, 320, 220, hwnd, (HMENU)ID_LISTBOX, hInst, NULL);
        SendMessageA(hList, WM_SETFONT, (WPARAM)hFont, TRUE);

        hStatus = CreateWindowA("STATIC", "Ready.", WS_CHILD | WS_VISIBLE | SS_SIMPLE, 10, 260, 320, 20, hwnd, (HMENU)ID_STATUS, hInst, NULL);
        SendMessageA(hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnSelectAll = CreateWindowA("BUTTON", "Select All", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 10, 285, 155, 30, hwnd, (HMENU)ID_BTN_SELECTALL, hInst, NULL);
        SendMessageA(hBtnSelectAll, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnPatch = CreateWindowA("BUTTON", "Check rw_data.dll", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 175, 285, 155, 30, hwnd, (HMENU)ID_BTN_PATCH, hInst, NULL);
        SendMessageA(hBtnPatch, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnExtract = CreateWindowA("BUTTON", "EXTRACT SELECTED", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 325, 320, 30, hwnd, (HMENU)ID_BTN_EXTRACT, hInst, NULL);
        SendMessageA(hBtnExtract, WM_SETFONT, (WPARAM)hFont, TRUE);

        hProgress = CreateWindowExA(0, "msctls_progress32", NULL, WS_CHILD | WS_VISIBLE, 10, 365, 320, 20, hwnd, (HMENU)ID_PROGRESS, hInst, NULL);

        hLblCredits = CreateWindowA("STATIC", "Created by Richard01_CZ", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 390, 320, 20, hwnd, (HMENU)ID_LBL_CREDITS, hInst, NULL);
        SendMessageA(hLblCredits, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        ScanCurrentDirectory(); UpdatePatchButtonState();
    } break;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        SetBkMode(hdc, TRANSPARENT);

        // Custom color for credits only
        if (hCtrl == hLblCredits) {
            SetTextColor(hdc, RGB(150, 150, 150));
        }

        return (LRESULT)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
    } break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_EXTRACT: ProcessExtraction(); break;
        case ID_BTN_SELECTALL: ToggleSelectAll(); break;
        case ID_BTN_PATCH: TogglePatchRwData(); break;
        } break;
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    InitCommonControls();

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MafiaDTAExtractorClass";
    wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(1));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    int windowWidth = 360;
    int windowHeight = 460;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExA(
        0,
        "MafiaDTAExtractorClass",
        "Mafia DTA Extractor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY,
        windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);
    MSG msg = { 0 };
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}