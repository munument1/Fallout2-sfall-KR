/*
 *    sfall
 *    Korean CP949 text renderer
 */

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "..\FalloutEngine\Fallout2.h"
#include "..\IniReader.h"
#include "..\main.h"
#include "..\SafeWrite.h"

#include "KoreanText.h"

namespace sfall
{

static bool enabled = false;
static long binaryThreshold = 128;

struct FontProfile {
	long tag;
	long fontSize;
	long lineHeight;
	long lineAdjust;
	long yOffset;
	long fontWeight;
	DWORD quality;
	wchar_t fontFace[LF_FACESIZE];
};

// Match the legacy Korean DLL layout used by the Fission-CE compatibility
// renderer: font 101 is laid out one pixel tighter while its full glyph is
// still drawn. Fonts 102/103 are buttons and 104 is the title face.
static FontProfile textProfile = {1, 11, 11, 1, 0, FW_NORMAL, NONANTIALIASED_QUALITY, L"Dotum"};
static FontProfile buttonProfile = {2, 15, 17, 0, 0, FW_NORMAL, ANTIALIASED_QUALITY, L"NanumBarunGothic"};
static FontProfile titleProfile = {3, 18, 20, 0, 0, FW_NORMAL, ANTIALIASED_QUALITY, L"NanumBarunGothic"};
static FontProfile* activeProfile = &textProfile;

struct GlyphBitmap {
	long width;
	long height;
	std::vector<BYTE> mask;
};

static std::unordered_map<unsigned long long, GlyphBitmap> glyphCache;
static HFONT hFont = nullptr;
static long hFontSize = 0;
static long hFontTag = 0;
static std::vector<std::wstring> loadedFontFiles;
static const char* langFontIni = ".\\LangFont.ini";

static bool IsCp949Lead(unsigned char ch) {
	if (ch == 0x95) return false; // Fallout UI console bullet.
	return ch >= 0x81 && ch <= 0xFE;
}

static bool IsCp949Trail(unsigned char ch) {
	return (ch >= 0x41 && ch <= 0x5A)
		|| (ch >= 0x61 && ch <= 0x7A)
		|| (ch >= 0x81 && ch <= 0xFE);
}

static bool DecodeCp949(const char* text, wchar_t& outChar) {
	unsigned char bytes[2] = {
		static_cast<unsigned char>(text[0]),
		static_cast<unsigned char>(text[1])
	};

	if (!IsCp949Lead(bytes[0]) || !IsCp949Trail(bytes[1])) return false;

	wchar_t wide[2] = {};
	int count = MultiByteToWideChar(949, MB_ERR_INVALID_CHARS,
		reinterpret_cast<const char*>(bytes), 2, wide, 2);
	if (count != 1) return false;

	outChar = wide[0];
	return true;
}

static fo::FontData* CurrentFont() {
	return reinterpret_cast<fo::FontData*>(fo::var::getInt(FO_VAR_gCurrentFont));
}

static long CurrentFontNumber() {
	return fo::var::getInt(FO_VAR_curr_font_num);
}

static FontProfile& ActiveProfile() {
	long font = CurrentFontNumber();
	if (font == 104) activeProfile = &titleProfile;
	else if (font == 102 || font == 103) activeProfile = &buttonProfile;
	else activeProfile = &textProfile;
	return *activeProfile;
}

static long KoreanLineHeight() {
	FontProfile& profile = ActiveProfile();
	long height = profile.lineHeight - profile.lineAdjust;
	return height > 0 ? height : 1;
}

static long CharSpacing() {
	__asm {
		call dword ptr ds:[FO_VAR_text_spacing];
	}
}

static long AsciiCharWidth(unsigned char ch) {
	fo::FontData* font = CurrentFont();
	if (!font) return 0;

	long width;
	if (ch == ' ') width = *reinterpret_cast<long*>(&font->field2);
	else width = *(reinterpret_cast<long*>(&font->fieldA) + (2 * ch));
	return width >> 16;
}

static void EnsureGdiFont() {
	FontProfile& profile = ActiveProfile();
	if (hFont && hFontSize == profile.fontSize && hFontTag == profile.tag) return;

	if (hFont) {
		DeleteObject(hFont);
		hFont = nullptr;
	}

	long weight = profile.fontWeight;
	if (weight < 0) weight = 0;
	if (weight > 1000) weight = 1000;

	hFontSize = profile.fontSize;
	hFontTag = profile.tag;
	hFont = CreateFontW(
		-profile.fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
		HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		profile.quality, DEFAULT_PITCH | FF_DONTCARE, profile.fontFace
	);
}

static GlyphBitmap& GetKoreanGlyph(wchar_t ch) {
	FontProfile& profile = ActiveProfile();
	unsigned long long key = (static_cast<unsigned long long>(profile.tag) << 32)
		| static_cast<DWORD>(ch);
	auto found = glyphCache.find(key);
	if (found != glyphCache.end()) return found->second;

	EnsureGdiFont();
	HDC dc = CreateCompatibleDC(nullptr);
	HGDIOBJ oldFont = SelectObject(dc, hFont);

	SIZE size = {};
	GetTextExtentPoint32W(dc, &ch, 1, &size);
	if (size.cx <= 0) size.cx = profile.fontSize;
	if (size.cy <= 0) size.cy = profile.fontSize;

	long bitmapWidth = std::max<long>(1, size.cx);
	long bitmapHeight = std::max<long>(profile.lineHeight,
		size.cy + std::abs(profile.yOffset));

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = bitmapWidth;
	bmi.bmiHeader.biHeight = -bitmapHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	HGDIOBJ oldBitmap = SelectObject(dc, dib);

	RECT rect = {0, 0, bitmapWidth, bitmapHeight};
	FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	SetBkMode(dc, OPAQUE);
	SetBkColor(dc, RGB(0, 0, 0));
	SetTextColor(dc, RGB(255, 255, 255));
	TextOutW(dc, 0, profile.yOffset, &ch, 1);

	GlyphBitmap glyph;
	glyph.width = bitmapWidth;
	glyph.height = bitmapHeight;
	glyph.mask.resize(static_cast<size_t>(bitmapWidth) * bitmapHeight);

	const BYTE* pixels = static_cast<const BYTE*>(bits);
	for (long y = 0; y < bitmapHeight; y++) {
		for (long x = 0; x < bitmapWidth; x++) {
			const BYTE* pixel = pixels + ((y * bitmapWidth + x) * 4);
			long luminance = std::max<long>(pixel[0], std::max<long>(pixel[1], pixel[2]));
			glyph.mask[y * bitmapWidth + x] = luminance >= binaryThreshold ? 9 : 0;
		}
	}

	SelectObject(dc, oldBitmap);
	DeleteObject(dib);
	SelectObject(dc, oldFont);
	DeleteDC(dc);

	auto inserted = glyphCache.emplace(key, std::move(glyph));
	return inserted.first->second;
}

static long KoreanCharWidth(wchar_t ch) {
	return GetKoreanGlyph(ch).width;
}

static bool IsLineBreak(unsigned char ch) {
	return ch == '\r' || ch == '\n';
}

static long __fastcall TextWidthHookImpl(const char* text) {
	long width = 0;
	long spacing = CharSpacing();

	while (*text) {
		unsigned char ch = static_cast<unsigned char>(*text);
		if (IsLineBreak(ch)) break;
		if (ch < 0x20) {
			if (ch == '\t') width += AsciiCharWidth(' ') + spacing;
			text++;
			continue;
		}

		wchar_t korean;
		if (DecodeCp949(text, korean)) {
			width += KoreanCharWidth(korean) + spacing;
			text += 2;
		} else {
			width += AsciiCharWidth(ch) + spacing;
			text++;
		}
	}

	return width;
}

static void DrawAsciiChar(BYTE*& dst, unsigned char ch, long pitch,
	fo::BlendColorTableData* blendTable) {
	fo::FontData* font = CurrentFont();
	if (!font) return;

	long width = AsciiCharWidth(ch);
	if (ch != ' ' && width > 0) {
		long height = (*(reinterpret_cast<long*>(&font->fieldC) + (2 * ch)) >> 16);
		BYTE* dstPixels = &dst[pitch * (font->field0 - height)];
		BYTE* fontPixels = reinterpret_cast<BYTE*>(
			*(reinterpret_cast<long*>(&font->eUnkArray[8 * ch + 2])) + font->field80C);

		for (long y = 0; y < height; y++) {
			for (long x = 0; x < width; x++) {
				*dstPixels = blendTable->data[*fontPixels].colors[*dstPixels];
				dstPixels++;
				fontPixels++;
			}
			dstPixels += pitch - width;
		}
	}

	dst += width + CharSpacing();
}

static void DrawKoreanChar(BYTE*& dst, wchar_t ch, long pitch,
	fo::BlendColorTableData* blendTable) {
	GlyphBitmap& glyph = GetKoreanGlyph(ch);
	BYTE* dstPixels = dst;

	// Raster height is intentionally independent from the reported line height.
	// This preserves the complete 11 px Dotum glyph with the legacy 10 px layout.
	for (long y = 0; y < glyph.height; y++) {
		for (long x = 0; x < glyph.width; x++) {
			BYTE mask = glyph.mask[y * glyph.width + x];
			if (mask) dstPixels[x] = blendTable->data[mask].colors[dstPixels[x]];
		}
		dstPixels += pitch;
	}

	dst += glyph.width + CharSpacing();
}

static void __stdcall TextToBufHookImpl(BYTE* dst, const char* text,
	long maxWidth, long pitch, long colorFlags) {
	if (!dst || !text || !*text || maxWidth <= 0 || pitch <= 0) return;

	fo::BlendColorTableData* blendTable = fo::func::getColorBlendTable(colorFlags);
	BYTE* cursor = dst;
	long spacing = CharSpacing();

	while (*text) {
		unsigned char ch = static_cast<unsigned char>(*text);
		if (IsLineBreak(ch)) break;
		if (ch < 0x20) {
			if (ch == '\t') ch = ' ';
			else {
				text++;
				continue;
			}
		}

		wchar_t korean;
		long charWidth;
		bool isKorean = DecodeCp949(text, korean);
		if (isKorean) charWidth = KoreanCharWidth(korean);
		else charWidth = AsciiCharWidth(ch);

		if ((cursor - dst) + charWidth > maxWidth) break;
		if (isKorean) {
			DrawKoreanChar(cursor, korean, pitch, blendTable);
			text += 2;
		} else {
			DrawAsciiChar(cursor, ch, pitch, blendTable);
			text++;
		}

		if ((cursor - dst) + spacing > maxWidth) break;
	}

	fo::func::freeColorBlendTable(colorFlags);
}

static long __stdcall TextHeightHookImpl() {
	return KoreanLineHeight();
}

static __declspec(naked) void text_height_hook() {
	__asm {
		push ecx;
		push edx;
		call TextHeightHookImpl;
		pop  edx;
		pop  ecx;
		retn;
	}
}

static __declspec(naked) void fm_text_width_hook() {
	__asm {
		push ecx;
		push edx;
		mov  ecx, eax;
		call TextWidthHookImpl;
		pop  edx;
		pop  ecx;
		retn;
	}
}

static __declspec(naked) void gnw_text_width_hook() {
	__asm {
		push ecx;
		push edx;
		mov  ecx, eax;
		call TextWidthHookImpl;
		pop  edx;
		pop  ecx;
		retn;
	}
}

static __declspec(naked) void fm_text_to_buf_hook() {
	__asm {
		push dword ptr [esp + 4]; // color flags
		push ecx;                 // surface pitch
		push ebx;                 // max text width
		push edx;                 // text
		push eax;                 // destination surface
		call TextToBufHookImpl;
		retn 4;
	}
}

static __declspec(naked) void gnw_text_to_buf_hook() {
	__asm {
		push dword ptr [esp + 4]; // color flags
		push ecx;                 // surface pitch
		push ebx;                 // max text width
		push edx;                 // text
		push eax;                 // destination surface
		call TextToBufHookImpl;
		retn 4;
	}
}

static int GetFontInt(const char* section, const char* key, int defaultValue) {
	return IniReader::GetInt(section, key, defaultValue, langFontIni);
}

static std::string GetFontString(const char* section, const char* key,
	const char* defaultValue) {
	return IniReader::GetString(section, key, defaultValue, langFontIni);
}

static int GetProfileInt(const char* section, const char* key,
	const char* legacyKey, int defaultValue) {
	int legacyValue = GetFontInt("Main", legacyKey, defaultValue);
	return GetFontInt(section, key, legacyValue);
}

static std::string GetProfileString(const char* section, const char* key,
	const char* legacyKey, const char* defaultValue) {
	auto legacyValue = GetFontString("Main", legacyKey, defaultValue);
	return GetFontString(section, key, legacyValue.c_str());
}

static std::wstring ToWideFontString(const std::string& value) {
	if (value.empty()) return std::wstring();

	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		value.c_str(), -1, nullptr, 0);
	UINT codePage = len > 0 ? CP_UTF8 : CP_ACP;
	len = MultiByteToWideChar(codePage, 0, value.c_str(), -1, nullptr, 0);
	if (len <= 0) return std::wstring();

	std::wstring wideValue(len, L'\0');
	MultiByteToWideChar(codePage, 0, value.c_str(), -1, &wideValue[0], len);
	wideValue.resize(len - 1);
	return wideValue;
}

static void LoadProfileFace(const char* section, const char* legacyKey,
	const char* defaultFace, wchar_t* dest) {
	auto face = GetProfileString(section, "Name", legacyKey, defaultFace);
	auto wideFace = ToWideFontString(face);
	std::memset(dest, 0, sizeof(wchar_t) * LF_FACESIZE);
	if (!wideFace.empty()) {
		wcsncpy_s(dest, LF_FACESIZE, wideFace.c_str(), LF_FACESIZE - 1);
	}
	dest[LF_FACESIZE - 1] = 0;
}

static void LoadPrivateFontFile(const char* section, const char* legacyKey) {
	auto path = GetProfileString(section, "File", legacyKey, "");
	if (path.empty()) return;

	auto widePath = ToWideFontString(path);
	if (widePath.empty()) return;
	if (std::find(loadedFontFiles.begin(), loadedFontFiles.end(), widePath)
		!= loadedFontFiles.end()) return;

	if (AddFontResourceExW(widePath.c_str(), FR_PRIVATE, nullptr) > 0) {
		loadedFontFiles.push_back(widePath);
	}
}

void KoreanText::init() {
	enabled = GetFontInt("MISC", "Enable",
		GetFontInt("Main", "KoreanText", 0)) != 0;
	if (!enabled || !nonEngLang) return;

	binaryThreshold = GetFontInt("MISC", "BinaryThreshold", 128);
	if (binaryThreshold < 1) binaryThreshold = 1;
	if (binaryThreshold > 255) binaryThreshold = 255;

	textProfile.fontSize = GetProfileInt("TEXT", "Size", "KoreanTextTextFontHeight", 11);
	textProfile.lineHeight = GetProfileInt("TEXT", "LineHeight", "KoreanTextTextRenderHeight", 11);
	textProfile.lineAdjust = GetProfileInt("TEXT", "Adjust", "KoreanTextTextAdjust", 1);
	textProfile.yOffset = GetProfileInt("TEXT", "YOffset", "KoreanTextTextYOffset", 0);
	textProfile.fontWeight = GetProfileInt("TEXT", "Weight", "KoreanTextTextFontWeight", FW_NORMAL);
	LoadPrivateFontFile("TEXT", "KoreanTextTextFontFile");
	LoadProfileFace("TEXT", "KoreanTextTextFontFace", "Dotum", textProfile.fontFace);

	buttonProfile.fontSize = GetProfileInt("BUTTON", "Size", "KoreanTextFontHeight", 15);
	buttonProfile.lineHeight = GetProfileInt("BUTTON", "LineHeight", "KoreanTextRenderHeight", 17);
	buttonProfile.lineAdjust = GetProfileInt("BUTTON", "Adjust", "KoreanTextAdjust", 0);
	buttonProfile.yOffset = GetProfileInt("BUTTON", "YOffset", "KoreanTextYOffset", 0);
	buttonProfile.fontWeight = GetProfileInt("BUTTON", "Weight", "KoreanTextFontWeight", FW_NORMAL);
	LoadPrivateFontFile("BUTTON", "KoreanTextFontFile");
	LoadProfileFace("BUTTON", "KoreanTextFontFace", "NanumBarunGothic", buttonProfile.fontFace);

	titleProfile.fontSize = GetProfileInt("GNW", "Size", "KoreanTextGnwFontHeight", 18);
	titleProfile.lineHeight = GetProfileInt("GNW", "LineHeight", "KoreanTextGnwRenderHeight", 20);
	titleProfile.lineAdjust = GetProfileInt("GNW", "Adjust", "KoreanTextGnwAdjust", 0);
	titleProfile.yOffset = GetProfileInt("GNW", "YOffset", "KoreanTextGnwYOffset", 0);
	titleProfile.fontWeight = GetProfileInt("GNW", "Weight", "KoreanTextGnwFontWeight", FW_NORMAL);
	LoadPrivateFontFile("GNW", "KoreanTextGnwFontFile");
	LoadProfileFace("GNW", "KoreanTextGnwFontFace", "NanumBarunGothic", titleProfile.fontFace);

	MakeJump(fo::funcoffs::FMtext_height_, text_height_hook);
	MakeJump(fo::funcoffs::GNW_text_height_, text_height_hook);
	MakeJump(fo::funcoffs::FMtext_width_, fm_text_width_hook);
	MakeJump(fo::funcoffs::GNW_text_width_, gnw_text_width_hook);
	MakeJump(fo::funcoffs::FMtext_to_buf_, fm_text_to_buf_hook);
	MakeJump(fo::funcoffs::GNW_text_to_buf_, gnw_text_to_buf_hook);
}

void KoreanText::exit() {
	glyphCache.clear();
	if (hFont) {
		DeleteObject(hFont);
		hFont = nullptr;
	}
	for (const auto& fontFile : loadedFontFiles) {
		RemoveFontResourceExW(fontFile.c_str(), FR_PRIVATE, nullptr);
	}
	loadedFontFiles.clear();
}

}
