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
