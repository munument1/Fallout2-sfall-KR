# Korean CP949 text renderer (experimental)

This branch starts from official sfall **4.5.1** and ports the existing Korean CP949 text renderer without changing the original Fallout 2 executable. It is a development branch, not a tested release.

## Usage

1. Build or obtain the branch's ReleaseXP `ddraw.dll` artifact from GitHub Actions.
2. Back up the installed `ddraw.dll` and existing `ddraw.ini`. Replace only `ddraw.dll`; keep the installed mod's `ddraw.ini`, `sfall.dat`, and other files unless they require their own updates.
3. Put `LangFont.ini` from `artifacts/` next to the DLL.
4. In your own `fallout2.cfg`, set `[system] language=korean` and provide CP949-encoded translation files under `data/text/korean`.
5. The renderer is enabled only when `[MISC] Enable=1` and the selected Fallout game configuration sets `language=korean` (case-insensitive). Do not copy an enabled `LangFont.ini` to unrelated language installations without testing.

The example font configuration uses Windows fonts Dotum and Malgun Gothic and does **not** bundle proprietary font files. Custom fonts may be configured using the per-profile `File` path.

## Known risks / tests required

- No in-game validation has been completed for this branch.
- Test Korean glyphs, text wrapping, character creation, inventory, Pip-Boy, dialogs, world map, saves and combat logs.
- Special 0x95 bullet bytes may conflict with valid CP949 lead bytes; test affected syllables and the message-log bullet before publishing.
- Check modded `ddraw.ini`, High Resolution Patch, Restoration Project, other translations, and Windows/Wine behavior separately.
- The DLL is a 32-bit Windows binary; test before use and do not replace a working installed version without backup.

Source of the Korean implementation: [munument1's Korean sfall fork](https://github.com/munument1/-KR-SFall-Unofficial-Translation) (GPL-3.0, sfall Team / Timeslip attribution retained).

## Code review findings (2026-10-15)

- The upstream 4.5.1 `GetPositionWidth` routine is preserved verbatim as `GetPositionWidthOriginal` and selected for every non-Korean configuration.
- `GetPositionWidthKorean` is selected only when `KoreanText::IsConfigured()` returns true: `language=korean`, `LangFont.ini [MISC] Enable=1`, and `versionCHI=false`.
- CP949 byte `0x95` is **not** reserved: for example `95 41` decodes to Korean character `뷒`. The old unconditional exclusion of `0x95` was removed. To avoid ambiguity with Fallout's legacy console bullet (`0x95`), the Korean console branch uses ASCII `*` as its bullet until a contextual renderer-safe marker is implemented.
- This is not a UTF-8 renderer. In-game translation files must currently use CP949; UTF-8 would require separate decoding and layout work.
- Local installed `fallout2.cfg` currently has `language=english`, and its legacy `ddraw.ini` identifies itself as v3.1. Do not replace that installation's files during development.

### Required regression cases

| Case | Expected result |
| --- | --- |
| Korean text containing `뷒` (CP949 `95 41`) | Render one Hangul character, not a bullet and an ASCII letter |
| Korean text with many syllables, English fragments, punctuation and line breaks | Wrap at glyph boundaries with no split lead/trail bytes |
| Console log starts with ASCII A/a or Hangul | The `*` prefix is independent of the first following character |
| `language=english`, `Enable=1` | Original 4.5.1 text wrapping and no Korean renderer hooks |
| `language=korean`, `Enable=0` | Original 4.5.1 text wrapping and no Korean renderer hooks |
| CJK-modified EXE detected by `versionCHI` | Keep existing official sfall compatibility path; no Korean hooks |
| High Resolution Patch on/off | No title, Pip-Boy or dialog clipping due to Korean glyph dimensions |

### Verification status

- [x] Static C++ source checks for selected-language guard, module registration, project membership and CP949 lead-byte handling.
- [x] Separate fork baseline branch `review/sfall-4.5.1-upstream-base` points to the exact upstream 4.5.1 commit.
- [ ] Compile with MSVC ReleaseXP Win32 and inspect errors/warnings.
- [ ] Produce a test-only `ddraw.dll` and verify file architecture/version.
- [ ] In-game screenshots and regression results for original English, Korean CP949 and modified Chinese/Japanese executables.

**Warning:** Changes are for review only; no test build or tested game release has been produced yet.
