# Sfall Korean GDI font renderer

This fork renders CP949/EUC-KR text through Windows GDI while retaining
Fallout 2's palette-based text output.

## Default profiles

| Fallout font | Use | Face | Pixel size | Reported line height |
| --- | --- | --- | ---: | ---: |
| 101 and other small fonts | Dialogue, logs, character editor | Dotum | 11 | 10 |
| 102, 103 | Buttons | NanumBarunGothic | 15 | 17 |
| 104 | Large titles | NanumBarunGothic | 18 | 20 |

Font 101 deliberately reports `LineHeight - Adjust` while drawing the full
11-pixel Dotum raster. This reproduces the row-layout behavior of the legacy
Korean `fallout2font.dll` and prevents lists from drifting outside their UI.

The small profile uses non-antialiased GDI output. Large profiles use GDI
antialiasing and then apply `BinaryThreshold` before writing Fallout palette
pixels, avoiding the blurred supersampling used by the earlier experiment.

## Runtime files

Copy these files and directories beside `fallout2.exe`:

- `ddraw.dll`
- `LangFont.ini`
- `fonts/NanumBarunGothic.ttf`
- `sfall.dat` and the normal sfall configuration files

The bundled font is loaded with `FR_PRIVATE`; it is not installed into
Windows. See `fonts/OFL.txt` for its license.

`LangFont.ini` controls font faces, sizes, line heights, offsets, weights,
the font-101 adjustment, and the binary raster threshold.
