# Gfx2Next

ZX Spectrum Next graphics conversion tool.

Converts an uncompressed 8-bit BMP or PNG file to the Sinclair ZX Spectrum Next graphics format(s).

## Supported Formats

* .nxb - Block
* .nxi - Bitmap
* .nxm - Map
* .nxp - Palette
* .nxt - Tiles
* .spr - Sprites
* .tmx - [Tiled](https://www.mapeditor.org/)

## Usage

gfx2next [options] &lt;srcfile&gt; [&lt;dstfile&gt;]

## Options

|Parameter|Description|
|---|---|
|-debug|Output additional debug information.|
|-font|Sets output to Next font format (.spr).|
|-bitmap|Sets output to Next bitmap mode (.nxi).|
|-bitmap-y|Get bitmap in Y order first. (Default is X order first).|
|-sprites|Sets output to Next sprite mode (.spr).|
|-tiles-file=&lt;filename&gt;|Load tiles from file in .nxt format.|
|-tile-size=XxY|Sets tile size to X x Y.|
|-tile-repeat|Remove repeating tiles.|
|-tile-rotate|Remove repeating, rotating and mirrored tiles.|
|-tile-y|Get tile in Y order first. (Default is X order first).|
|-tile-ldws|Get tile in Y order first for ldws instruction. (Default is X order first).|
|-tiled-file=&lt;filename&gt;|Load map from file in .tmx format.|
|-tiled-blank=X|Set the tile id of the blank tile.|
|-block-size=XxY|Sets blocks size to X x Y for blocks of tiles.|
|-block-size=n|Sets blocks size to n bytes for blocks of tiles.|
|-block-repeat|Remove repeating blocks.|
|-block-16bit|Get blocks as 16 bit index for &lt; 256 blocks.|
|-map-none|Don't save a map file (e.g. if you're just adding to tiles).|
|-map-16bit|Save map as 16 bit output.|
|-map-y|Save map in Y order first. (Default is X order first).|
|-bank-8k|Splits up output file into multiple 8k files.|
|-bank-16k|Splits up output file into multiple 16k files.|
|-bank-48k|Splits up output file into multiple 48k files.|
|-bank-size=XxY|Splits up output file into multiple X x Y files.|
|-bank-sections=name,...|Section names for asm files.|
|-color-distance|Use the shortest distance between color values (default).|
|-color-floor|Round down the color values to the nearest integer.|
|-color-ceil|Round up the color values to the nearest integer.|
|-color-round|Round the color values to the nearest integer.|
|-colors-4bit|Use 4 bits per pixel (16 colors). Default is 8 bits per pixel (256 colors). Get sprites or tiles as 16 colors, top 4 bits of 16 bit map is palette index.|
|-pal-file=&lt;filename&gt;|Load palette from file in .nxp format.|
|-pal-embed|The raw palette is prepended to the raw image file.|
|-pal-ext|The raw palette is written to an external file (.nxp). This is the default.|
|-pal-min|If specified, minimize the palette by removing any duplicated colors, sort it in ascending order, and clear any unused palette entries at the end. This option is ignored if the -pal-std option is given.|
|-pal-std|If specified, convert to the Spectrum Next standard palette colors. This option is ignored if the -colors-4bit option is given.|
|-pal-none|No raw palette is created.|
|-zx0|Compress the image data using zx0.|
|-zx0-back|Compress the image data using zx0 in reverse.|
|-zx7|Compress the image data using zx7.|
|-zx7-back|Compress the image data using zx7 in reverse.|
|-megalz|Compress the image data using MegaLZ optimal.|
|-megalz-greedy|Compress the image data using MegaLZ greedy.|
|-asm-z80asm|Generate header and asm binary include files (in Z80ASM format).|
|-asm-sjasm|Generate asm binary incbin file (SjASM format).|
|-asm-file=&lt;name&gt;|Append asm and header output to &lt;name&gt;.asm and &lt;name&gt;.h.|
|-asm-start|Specifies the start of the asm and header data for appending.|
|-asm-end|Specifies the end of the asm and header data for appending.|
|-asm-sequence|Add sequence section for multi-bank spanning data.|
|-preview|Generate png preview file(s).|

## Examples
* gfx2next.exe -tile-repeat -map-16bit -bank-16k -asm-z80asm -bank-sections=rodata_user,rodata_user,BANK_52,BANK_53,rodata_user -preview tiles.png
* gfx2next.exe -tile-repeat -map-16bit -colors-4bit -asm-z80asm -bank-sections=rodata_user,BANK_17,BANK_17 tiles.png
* gfx2next.exe -sprites -colors-4bit -pal-min -pal-ext -preview sprites.png
* gfx2next.exe -bitmap -pal-std -preview titlescreen.png

## Source code
https://github.com/headkaze/Gfx2Next

## Compiling
gcc -O2 -Wall -o bin/gfx2next src/lodepng.c src/zx0.c src/zx7.c src/megalz.c src/gfx2next.c

## Credits

* Ben Baker - [Gfx2Next](https://www.rustypixels.uk/?page_id=976) Author & Maintainer
* Antonio Villena - ZX7b
* Einar Saukas - ZX0 / ZX7
* Jim Bagley - NextGrab / MapGrabber
* Michael Ware - [Tiled2Bin](https://www.rustypixels.uk/?page_id=739)
* Stefan Bylun - [NextBmp / NextRaw](https://github.com/stefanbylund/zxnext_bmp_tools)
* fyrex^mhm - MegaLZ
* lvd^mhm - MegaLZ
* Lode Vandevenne - [LodePNG](https://lodev.org/lodepng/)

## Download
[Gfx2Next](https://www.rustypixels.uk/?download=1001)
