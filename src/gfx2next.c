/*******************************************************************************
 * Gfx2Next - ZX Spectrum Next graphics conversion tool
 * 
 * Credits:
 *
 *    Ben Baker - [Gfx2Next](https://www.rustypixels.uk/?page_id=976) Author & Maintainer
 *    Craig Hackney - Added -pal-rgb332 & -pal-bgr222, -colors-1bit, and -map-sms options and some fixes
 *    Einar Saukas - [ZX0](https://github.com/einar-saukas/ZX0)
 *    Jim Bagley - NextGrab / MapGrabber
 *    Juan J. Martinez - [png2scr](https://github.com/reidrac/png2scr)
 *    Lode Vandevenne - [LodePNG](https://lodev.org/lodepng/)
 *    Michael Ware - [Tiled2Bin](https://www.rustypixels.uk/?page_id=739)
 *    Stefan Bylund - [NextBmp / NextRaw](https://github.com/stefanbylund/zxnext_bmp_tools)
 *
 * Supports the following ZX Spectrum Next formats:
 *
 *    .nxb - Block
 *    .nxi - Bitmap
 *    .nxm - Map
 *    .nxp - Palette
 *    .nxt - Tiles
 *    .spr - Sprites
 *    .scr - Screens
 *    .tmx - [Tiled](https://www.mapeditor.org/)
 *
 ******************************************************************************/

int _CRT_glob = 0;

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <glob.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include "zx0.h"
#include "lodepng.h"

#define VERSION						"1.1.7"

#define DIR_SEPERATOR_CHAR			'\\'

#define BMP_FILE_HEADER_SIZE		14
#define BMP_MIN_DIB_HEADER_SIZE		40
#define BMP_HEADER_SIZE				54
#define BMP_MIN_FILE_SIZE			1082

#define PALETTE_SIZE				1024

#define NEXT_PALETTE_SIZE 			512
#define NEXT_4BIT_PALETTE_SIZE		32

#define NUM_PALETTE_COLORS			256
#define MAX_LABEL_COUNT				8
#define MAX_BANK_SECTION_COUNT		8

#define TILES_SIZE					262144 * 256
#define MAP_SIZE					1024 * 1024
#define BLOCK_SIZE					1024 * 1024
#define NUM_BANKS					256

#define SIZE_8K						8192
#define SIZE_16K					16384
#define SIZE_48K					49152

#define COLOR_BLACK					0x000000
#define COLOR_BLUE					0x0000D7
#define COLOR_RED					0xD70000
#define COLOR_MAGENTA				0xD700D7
#define COLOR_GREEN					0x00D700
#define COLOR_CYAN					0x00D7D7
#define COLOR_YELLOW				0xD7D700
#define COLOR_WHITE					0xD7D7D7

#define COLOR_BLUE_BRIGHT			0x0000FF
#define COLOR_RED_BRIGHT			0xFF0000
#define COLOR_MAGENTA_BRIGHT		0xFF00FF
#define COLOR_GREEN_BRIGHT			0x00FF00
#define COLOR_CYAN_BRIGHT			0x00FFFF
#define COLOR_YELLOW_BRIGHT			0xFFFF00
#define COLOR_WHITE_BRIGHT			0xFFFFFF

#define TILED_DIAG					(1 << 1)
#define TILED_VERT					(1 << 2)
#define TILED_HORIZ					(1 << 3)
#define TILED_HORIZ_VERT			(TILED_HORIZ | TILED_VERT)
#define TILED_TILEID_MASK			0x1FFFFFFF

#define DBL_MAX						1.7976931348623158e+308

#define RGB888(r8,g8,b8)			((r8 << 16) | (g8 << 8) | b8)
#define RGB332(r3,g3,b2)			((r3 << 5) | (g3 << 2) | b2)
#define RGB333(r3,g3,b3)			((r3 << 6) | (g3 << 3) | b3)
#define BGR222(b2,g2,r2)			((b2 << 4) | (g2 << 2) | r2)

#define MIN(x,y)					((x) < (y) ? (x) : (y))
#define MAX(x,y)					((x) > (y) ? (x) : (y))

#define EXT_ZX0						".zx0"

#define EXT_BIN						".bin"
#define EXT_NXM						".nxm"
#define EXT_NXP						".nxp"
#define EXT_NXT						".nxt"
#define EXT_SPR						".spr"
#define EXT_NXB						".nxb"
#define EXT_NXI						".nxi"
#define EXT_SCR						".scr"
#define EXT_TMX						".tmx"
#define EXT_TSX						".tsx"

static void write_easter_egg();
static uint8_t attributes_to_tiled_flags(uint8_t attributes);

unsigned char m_gf[] =
{
	0x84, 0xff, 0x42, 0x7c, 0xf8, 0x00, 0x07, 0xe0, 0x2a, 0xc0, 0x24, 0x00,
	0x3f, 0x38, 0xc5, 0xe4, 0xf8, 0x01, 0xe2, 0xba, 0xfc, 0xc6, 0xe4, 0x80,
	0xbe, 0xbf, 0xf0, 0xd8, 0x92, 0x7c, 0xfa, 0xe3, 0x38, 0xc1, 0x12, 0x86,
	0x87, 0xe0, 0x7f, 0xe0, 0xbe, 0x1f, 0xf4, 0x87, 0xfe, 0x8f, 0xec, 0x6a,
	0xe8, 0x43, 0x8f, 0x1e, 0xac, 0x6c, 0xc7, 0xda, 0x96, 0xf8, 0x4a, 0xf1,
	0x02, 0x97, 0xf1, 0xc3, 0x22, 0xa7, 0x93, 0xfe, 0xc2, 0x8e, 0xf2, 0x3f,
	0xf0, 0x96, 0xcf, 0x95, 0x9f, 0xbd, 0xdf, 0xb0, 0x7d, 0xe0, 0x2f, 0xe7,
	0x50, 0x7d, 0xe0, 0x29, 0xff, 0x69, 0xf3, 0x29, 0xfc, 0x02, 0x92, 0xfb,
	0x90, 0xfd, 0x2f, 0xf9, 0xf2, 0x0f, 0xce, 0x7f, 0xb3, 0xe0, 0x15, 0x29,
	0xf3, 0x5a, 0xf7, 0x50, 0x4b, 0xf3, 0x1c, 0xc7, 0x92, 0xe0, 0xa6, 0x83,
	0xf0, 0x00, 0x3f, 0x07, 0xe7, 0xe6, 0xe0, 0x00, 0x0f, 0x96, 0xe0, 0x09,
	0x1f, 0xf0, 0x0f, 0x83, 0x1e, 0x7b, 0xc8, 0xc1, 0x20, 0x6a, 0xf1, 0x8a,
	0xdf, 0xf8, 0x43, 0xc7, 0xe1, 0x38, 0xfc, 0xb9, 0xe0, 0xe9, 0xc8, 0xc6,
	0xc0, 0x0f, 0xbf, 0x9f, 0x92, 0xc0, 0x68, 0xfb, 0x00, 0x5f, 0x9a, 0x80,
	0x07, 0xbe, 0x1e, 0x8b, 0xe2, 0x30, 0xe1, 0x3c, 0x03, 0x62, 0x7f, 0x9f,
	0x4a, 0x60, 0x29, 0x1c, 0x98, 0xfe, 0xf1, 0xdf, 0x66, 0xfb, 0xe1, 0x9f,
	0x76, 0x61, 0x65, 0xfd, 0xe0, 0xcf, 0xa7, 0xbf, 0xa2, 0xe0, 0xde, 0x6f,
	0x46, 0x65, 0xfb, 0xbf, 0xa7, 0x22, 0x3f, 0xf7, 0x47, 0xf1, 0x41, 0xe0,
	0x99, 0xfe, 0x5f, 0x77, 0x69, 0x3f, 0x22, 0x7f, 0x67, 0x5a, 0x7f, 0x4a,
	0xbf, 0x52, 0x65, 0xfc, 0xdf, 0x6f, 0x22, 0xfe, 0x4f, 0x4a, 0xfe, 0x4a,
	0xfe, 0xa5, 0xdf, 0xa5, 0xff, 0xa1, 0x9f, 0xa0, 0xf3, 0x3c, 0x10, 0x8f,
	0xf7, 0xfc, 0xbb, 0xe6, 0xe0, 0xfd, 0xdf, 0x3f, 0x02, 0x92, 0xfc, 0x80,
	0x7f, 0xfe, 0x5c, 0xf7, 0xe6, 0x3a, 0xe0, 0x9a, 0xf3, 0x3f, 0x62, 0xf3,
	0xe6, 0x4a, 0xfb, 0x23, 0x9e, 0xbf, 0xeb, 0xb0, 0xcc, 0xd8, 0x00, 0xfa,
	0x9e, 0xdf, 0xe0, 0xf9, 0x06, 0x26, 0x7f, 0xef, 0x25, 0xe7, 0xf1, 0xb0,
	0xfd, 0xbe, 0x86, 0xe0, 0x97, 0xe3, 0xc8, 0xb0, 0xf2, 0xef, 0x8f, 0xe0,
	0x96, 0xf3, 0x94, 0x1f, 0x89, 0xe8, 0x3f, 0x69, 0xe7, 0x22, 0xc0, 0x7f,
	0x48, 0x92, 0xf9, 0xef, 0x85, 0xcc, 0xa6, 0xfe, 0x96, 0xde, 0x26, 0xfd,
	0xcf, 0x9a, 0x7f, 0x3f, 0x5c, 0xee, 0xc6, 0x3f, 0x41, 0x96, 0x9e, 0xfa,
	0xfc, 0xe0, 0xbf, 0x22, 0xbe, 0x0f, 0x4d, 0xce, 0x92, 0xdb, 0xf6, 0x07,
	0xe0, 0x66, 0x80, 0x00, 0x7f, 0x25, 0x3f, 0x03, 0xbe, 0x7e, 0xe5, 0x8a,
	0xda, 0x15, 0x01, 0x0e, 0x48, 0x6f, 0xed, 0xf2, 0xb9, 0x00, 0xe0, 0xa6,
	0xff, 0x9a, 0xfd, 0xff, 0xa5, 0x3f, 0xa4, 0xf1, 0xbc, 0x80, 0x3e, 0xf3,
	0xd2, 0xa4, 0xe1, 0x07, 0xa0, 0xbf, 0x29, 0x03, 0x5a, 0x00, 0x4a, 0xdf,
	0x06, 0x92, 0x3f, 0x80, 0xfd, 0xa1, 0x01, 0x99, 0xef, 0xf0, 0x3f, 0x7f,
	0xbf, 0xe0, 0xa1, 0xcf, 0x3e, 0x1d, 0x33, 0x9f, 0xf6, 0xe0, 0x69, 0x01,
	0xa1, 0xc0, 0x3e, 0x9e, 0x39, 0xfe, 0x00, 0xe0, 0x6a, 0x0f, 0x91, 0xf8,
	0xaa, 0x03, 0xe0, 0xa5, 0x03, 0xa6, 0x00, 0xa9, 0x00, 0x00, 0x4a, 0xfc,
	0x29, 0x01, 0x0a, 0xe0, 0x29, 0x00, 0x28, 0xfe, 0x69, 0x80, 0xa1, 0x7f,
	0x69, 0x00, 0xa5, 0x1f, 0x68, 0x07, 0x68, 0xfc, 0x19, 0x94, 0x00, 0xf8,
	0x03, 0xfc, 0x3d, 0xe0, 0x3f, 0xe3, 0xe0, 0xa0, 0xf8, 0x62, 0x02, 0x7f,
	0x56, 0x94, 0x3f, 0x89, 0x04, 0x9f, 0x48, 0x82, 0x06, 0xef, 0x84, 0xe8,
	0xa0, 0xf7, 0xa1, 0xe0, 0x2f, 0xf9, 0x80, 0x4a, 0xfc, 0x52, 0x66, 0x04,
	0xfe, 0x7f, 0x84, 0xf0, 0xbe, 0xff, 0xe0, 0x81, 0x90, 0xaa, 0x04, 0xff,
	0x69, 0xa0, 0x5a, 0x60, 0x06, 0x86, 0x06, 0x94, 0x40, 0x89, 0xfe, 0x80,
	0x06, 0xd9, 0x0e, 0xa2, 0x30, 0x00, 0x08
};

uint32_t m_screenColors[] =
{
	COLOR_BLACK,
	COLOR_BLUE,
	COLOR_BLUE_BRIGHT,
	COLOR_RED,
	COLOR_RED_BRIGHT,
	COLOR_MAGENTA,
	COLOR_MAGENTA_BRIGHT,
	COLOR_GREEN,
	COLOR_GREEN_BRIGHT,
	COLOR_CYAN,
	COLOR_CYAN_BRIGHT,
	COLOR_YELLOW,
	COLOR_YELLOW_BRIGHT,
	COLOR_WHITE,
	COLOR_WHITE_BRIGHT
};

uint8_t m_screenAttribsPaper[] =
{
	0x00, 0x08, 0x08 | 0x40, 0x10, 0x10 | 0x40, 0x18, 0x18 | 0x40, 0x20, 0x20 | 0x40, 0x28, 0x28 | 0x40, 0x30, 0x30 | 0x40, 0x38, 0x38 | 0x40
};

uint8_t m_screenAttribsInk[] =
{
	0x00, 0x01, 0x01 | 0x40, 0x02, 0x02 | 0x40, 0x03, 0x03 | 0x40, 0x04, 0x04 | 0x40, 0x05, 0x05 | 0x40, 0x06, 0x06 | 0x40, 0x07, 0x07 | 0x40
};

typedef enum
{
	COLORMODE_DISTANCE,
	COLORMODE_ROUND,
	COLORMODE_FLOOR,
	COLORMODE_CEIL
} color_mode_t;

typedef enum
{
	PALMODE_NONE,
	PALMODE_EMBEDDED,
	PALMODE_EXTERNAL
} pal_mode_t;

typedef enum
{
	ASMMODE_NONE,
	ASMMODE_SJASM,
	ASMMODE_Z80ASM
} asm_mode_t;

typedef enum
{
	BANKSIZE_NONE,
	BANKSIZE_8K,
	BANKSIZE_16K,
	BANKSIZE_48K,
	BANKSIZE_CUSTOM
} bank_size_t;

typedef enum
{
	MATCH_NONE = 0,
	MATCH_XY = (1 << 0),
	MATCH_ROTATE = (1 << 1),
	MATCH_MIRROR_Y = (1 << 2),
	MATCH_MIRROR_X = (1 << 3),
	MATCH_MIRROR_XY = (1 << 4),
	MATCH_ANY = MATCH_XY | MATCH_MIRROR_Y | MATCH_MIRROR_X | MATCH_MIRROR_XY
} match_t;

typedef enum
{
	COMPRESS_NONE = 0,
	COMPRESS_SCREEN = (1 << 0),
	COMPRESS_BITMAP = (1 << 1),
	COMPRESS_SPRITES = (1 << 2),
	COMPRESS_TILES = (1 << 3),
	COMPRESS_BLOCKS = (1 << 4),
	COMPRESS_MAP = (1 << 5),
	COMPRESS_PALETTE = (1 << 6),
	COMPRESS_ALL = COMPRESS_SCREEN | COMPRESS_BITMAP | COMPRESS_SPRITES | COMPRESS_TILES | COMPRESS_BLOCKS | COMPRESS_MAP | COMPRESS_PALETTE
} compress_t;

typedef struct
{
	char *in_filename;
	char *out_filename;
	bool debug;
	bool font;
	bool screen;
	bool screen_attribs;
	bool bitmap;
	bool bitmap_y;
	bool sprites;
	char *tiles_file;
	bool tile_norepeat;
	bool tile_nomirror;
	bool tile_norotate;
	bool tile_y;
	bool tile_ldws;
	int tile_offset;
	bool tile_offset_auto;
	int tile_pal;
	bool tile_pal_auto;
	bool tile_none;
	bool tile_planar4;
	bool tiled;
	bool tiled_tsx;
	char *tiled_file;
	int tiled_blank;
	bool tiled_output;
	int tiled_width;
	bool block_norepeat;
	bool block_16bit;
	bool map_none;
	bool map_16bit;
	bool map_y;
	bool map_sms;
	bank_size_t bank_size;
	color_mode_t color_mode;
	bool colors_4bit;
	bool colors_1bit;
	char *pal_file;
	pal_mode_t pal_mode;
	bool pal_min;
	bool pal_full;
	bool pal_std;
	bool pal_rgb332;
	bool pal_bgr222;
	bool zx0_back;
	bool zx0_quick;
	compress_t compress;
	asm_mode_t asm_mode;
	char *asm_file;
	bool asm_start;
	bool asm_start_auto;
	bool asm_end;
	bool asm_end_auto;
	bool asm_sequence;
	bool preview;
} arguments_t;

static arguments_t m_args  =
{
	.in_filename = NULL,
	.out_filename = NULL,
	.debug = false,
	.font = false,
	.screen = false,
	.screen_attribs = false,
	.bitmap = false,
	.bitmap_y = false,
	.sprites = false,
	.tiles_file = NULL,
	.tile_norepeat = false,
	.tile_nomirror = false,
	.tile_norotate = false,
	.tile_y = false,
	.tile_ldws = false,
	.tile_offset = 0,
	.tile_offset_auto = false,
	.tile_pal = 0,
	.tile_pal_auto = false,
	.tile_none = false,
	.tile_planar4 = false,
	.tiled = false,
	.tiled_tsx = false,
	.tiled_file = NULL,
	.tiled_blank = 0,
	.tiled_output = false,
	.tiled_width = 256,
	.block_norepeat = false,
	.block_16bit = false,
	.map_none = false,
	.map_16bit = false,
	.map_y = false,
	.map_sms = false,
	.bank_size = BANKSIZE_NONE,
	.color_mode = COLORMODE_DISTANCE,
	.colors_4bit = false,
	.colors_1bit = false,
	.pal_file = NULL,
	.pal_mode = PALMODE_EXTERNAL,
	.pal_min = false,
	.pal_full = false,
	.pal_std = false,
	.pal_rgb332 = false,
	.pal_bgr222 = false,
	.zx0_back = false,
	.zx0_quick = false,
	.compress = COMPRESS_NONE,
	.asm_mode = ASMMODE_NONE,
	.asm_file = NULL,
	.asm_start = false,
	.asm_start_auto = false,
	.asm_end = false,
	.asm_end_auto = false,
	.asm_sequence = false,
	.preview = false,
};

static uint8_t m_bmp_header[BMP_HEADER_SIZE] = { 0 };

static uint8_t m_palette[PALETTE_SIZE] = { 0 };
static uint8_t m_min_palette[PALETTE_SIZE] = { 0 };
static uint16_t m_next_palette[NEXT_PALETTE_SIZE / 2] = { 0 };

static uint8_t m_min_palette_index[NUM_PALETTE_COLORS] = { 0 };
static uint8_t m_std_palette_index[NUM_PALETTE_COLORS] = { 0 };

static uint8_t m_tiles[TILES_SIZE] = { 0 };
static uint16_t m_map[MAP_SIZE] = { 0 };
static uint16_t m_blocks[BLOCK_SIZE] = { 0 };

static uint8_t *m_image = NULL;
static uint32_t m_image_width = 0;
static int32_t m_image_height = 0;
static uint32_t m_image_size = 0;

static uint8_t *m_next_image = NULL;
static uint32_t m_next_image_width = 0;
static uint32_t m_next_image_size = 0;

static uint32_t m_padded_image_width = 0;
static bool m_bottom_to_top_image = false;

static uint32_t m_bank_index = 0;
static uint32_t m_bank_size = 0;
static uint32_t m_bank_count = 0;

static char m_bank_sections[MAX_BANK_SECTION_COUNT][256] = { { 0 } };
static uint32_t m_bank_used[NUM_BANKS] = { 0 };
static uint32_t m_bank_section_index = 0;
static uint32_t m_bank_section_count = 0;

static uint32_t m_bitmap_width = 0;
static uint32_t m_bitmap_height = 0;

static uint32_t m_tile_width = 8;
static uint32_t m_tile_height = 8;
static uint32_t m_tile_size = 8 * 8;
static uint32_t m_tile_count = 0;

static uint32_t m_block_width = 1;
static uint32_t m_block_height = 1;
static uint32_t m_block_size = 1 * 1;
static uint32_t m_block_count = 0;

static uint32_t m_chunk_size = 0;

static char m_bitmap_filename[256] = { 0 };
static char m_asm_labels[MAX_LABEL_COUNT][256] = { { 0 } };

static FILE *m_bitmap_file = NULL;
static FILE *m_asm_file = NULL;
static FILE *m_header_file = NULL;

static void close_all(void)
{
	if (m_image != NULL)
	{
		free(m_image);
		m_image = NULL;
	}

	if (m_next_image != NULL)
	{
		free(m_next_image);
		m_next_image = NULL;
	}
	
	if (m_bitmap_file != NULL)
	{
		fclose(m_bitmap_file);
		m_bitmap_file = NULL;
	}
	
	if (m_asm_file != NULL)
	{
		fclose(m_asm_file);
		m_asm_file = NULL;
	}
	
	if (m_header_file != NULL)
	{
		fclose(m_header_file);
		m_header_file = NULL;
	}
}

static void exit_handler(void)
{
	close_all();
}

static void exit_with_msg(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	exit(EXIT_FAILURE);
}

static uint8_t c8_to_c3(uint8_t c8, color_mode_t color_mode)
{
	double c3 = (c8 * 7.0) / 255.0;

	switch (color_mode)
	{
	case COLORMODE_FLOOR:
		return (uint8_t) floor(c3);
	case COLORMODE_CEIL:
		return (uint8_t) ceil(c3);
	case COLORMODE_ROUND:
	case COLORMODE_DISTANCE:
	// Fall through
	default:
		return (uint8_t) round(c3);
	}
}

static uint8_t c8_to_c2(uint8_t c8, color_mode_t color_mode)
{
	double c2 = (c8 * 3.0) / 255.0;

	switch (color_mode)
	{
	case COLORMODE_FLOOR:
		return (uint8_t) floor(c2);
	case COLORMODE_CEIL:
		return (uint8_t) ceil(c2);
	case COLORMODE_ROUND:
	case COLORMODE_DISTANCE:
	// Fall through
	default:
		return (uint8_t) round(c2);
	}
}

static uint8_t c2_to_c3(uint8_t c2)
{
	return (c2 << 1) | (((c2 >> 1) | c2) & 1);
}

static uint8_t c3_to_c8(uint8_t c3)
{
	return (uint8_t) round((c3 * 255.0) / 7.0);
}

static uint32_t rgb332_to_rgb888(uint16_t rgb333)
{
	uint8_t r3 = (uint8_t) ((rgb333 >> 5) & 7);
	uint8_t g3 = (uint8_t) ((rgb333 >> 2) & 7);
	uint8_t b2 = (uint8_t) (rgb333 & 3);
	uint8_t b3 = c2_to_c3(b2);
	uint8_t r = c3_to_c8(r3);
	uint8_t g = c3_to_c8(g3);
	uint8_t b = c3_to_c8(b3);
	return RGB888(r, g, b);
}

static uint32_t rgb333_to_rgb888(uint16_t rgb333)
{
	uint8_t r3 = (uint8_t) ((rgb333 >> 6) & 7);
	uint8_t g3 = (uint8_t) ((rgb333 >> 3) & 7);
	uint8_t b3 = (uint8_t) (rgb333 & 7);
	uint8_t r = c3_to_c8(r3);
	uint8_t g = c3_to_c8(g3);
	uint8_t b = c3_to_c8(b3);
	return RGB888(r, g, b);
}

static uint16_t rgb888_to_rgb332(uint32_t rgb888, color_mode_t color_mode)
{
	uint8_t r8 = rgb888 >> 16;
	uint8_t g8 = rgb888 >> 8;
	uint8_t b8 = rgb888;
	uint8_t r3 = c8_to_c3(r8, color_mode);
	uint8_t g3 = c8_to_c3(g8, color_mode);
	uint8_t b2 = c8_to_c2(b8, color_mode);
	return RGB332(r3, g3, b2);
}

static uint16_t rgb888_to_bgr222(uint32_t rgb888, color_mode_t color_mode)
{
	uint8_t r8 = rgb888 >> 16;
	uint8_t g8 = rgb888 >> 8;
	uint8_t b8 = rgb888;
	uint8_t r2 = c8_to_c2(r8, color_mode);
	uint8_t g2 = c8_to_c2(g8, color_mode);
	uint8_t b2 = c8_to_c2(b8, color_mode);
	return BGR222(b2, g2, r2);
}

static uint16_t rgb888_to_rgb333(uint32_t rgb888, color_mode_t color_mode)
{
	uint8_t r8 = rgb888 >> 16;
	uint8_t g8 = rgb888 >> 8;
	uint8_t b8 = rgb888;
	uint8_t r3 = c8_to_c3(r8, color_mode);
	uint8_t g3 = c8_to_c3(g8, color_mode);
	uint8_t b3 = c8_to_c3(b8, color_mode);
	return RGB333(r3, g3, b3);
}

static uint8_t get_screen_color_attribs(uint32_t rgb888, bool useInk)
{
	int index = 0;
	
	for (; index < 15; index++)
	{
		if (rgb888 == m_screenColors[index])
			break;
	}
	
	return (useInk ? m_screenAttribsInk[index] : m_screenAttribsPaper[index]);
}

static uint32_t get_nearest_screen_color(uint32_t rgb888)
{
	uint32_t match = 0;
	double min_dist = DBL_MAX;
	uint8_t r = rgb888 >> 16;
	uint8_t g = rgb888 >> 8;
	uint8_t b = rgb888;
	
	for (int i = 0; i < 15; i++)
	{
		uint32_t rgb888_pal = m_screenColors[i];
		uint8_t rp = rgb888_pal >> 16;
		uint8_t gp = rgb888_pal >> 8;
		uint8_t bp = rgb888_pal;
		double dist = sqrt(pow(rp - r, 2) + pow(gp - g, 2) + pow(bp - b, 2));

		if (dist < min_dist)
		{
			match = rgb888_pal;
			min_dist = dist;

			if (dist == 0)
				return rgb888_pal;
		}
	}
	
	return match;
}

static uint32_t get_nearest_color(uint32_t rgb888, bool use_333)
{
	uint32_t match = 0;
	double min_dist = DBL_MAX;
	uint8_t r = rgb888 >> 16;
	uint8_t g = rgb888 >> 8;
	uint8_t b = rgb888;
	uint32_t num_palette_colors = use_333 ? 512 : 256;

	for (int i = 0; i < num_palette_colors; i++)
	{
		uint32_t rgb888_pal = use_333 ? rgb333_to_rgb888(i) : rgb332_to_rgb888(i);
		uint8_t rp = rgb888_pal >> 16;
		uint8_t gp = rgb888_pal >> 8;
		uint8_t bp = rgb888_pal;
		
		double dist = sqrt(pow(rp - r, 2) + pow(gp - g, 2) + pow(bp - b, 2));

		if (dist < min_dist)
		{
			match = rgb888_pal;
			min_dist = dist;

			if (dist == 0)
				return rgb888_pal;
		}
	}

	return match;
}

static void convert_palette(color_mode_t color_mode)
{
	// Update the colors in the palette.
	// The original RGB888 colors in the palette are converted to
	// RGB333 colors and then back to their equivalent RGB888 colors.
	for (int i = 0; i < NUM_PALETTE_COLORS; i++)
	{
		// Palette contains ARGB colors.
		uint8_t r8 = m_palette[i * 4 + 1];
		uint8_t g8 = m_palette[i * 4 + 2];
		uint8_t b8 = m_palette[i * 4 + 3];

		if (color_mode == COLORMODE_DISTANCE)
		{
			uint32_t rgb888 =  get_nearest_color(RGB888(r8, g8, b8), true);
			
			m_palette[i * 4 + 0] = 0;
			m_palette[i * 4 + 1] = (rgb888 >> 16);
			m_palette[i * 4 + 2] = (rgb888 >> 8);
			m_palette[i * 4 + 3] = rgb888;
		}
		else
		{
			uint32_t rgb888 = rgb333_to_rgb888(rgb888_to_rgb333(RGB888(r8, g8, b8), color_mode));
			
			m_palette[i * 4 + 0] = 0;
			m_palette[i * 4 + 1] = (rgb888 >> 16);
			m_palette[i * 4 + 2] = (rgb888 >> 8);
			m_palette[i * 4 + 3] = rgb888;
		}
	}
}

static void convert_standard_palette(color_mode_t color_mode)
{
	// Update the colors in the palette.
	// The original RGB888 colors in the palette are converted to the RGB332/
	// RGB333 colors in the standard palette and then back to their equivalent
	// RGB888 colors.
	for (int i = 0; i < NUM_PALETTE_COLORS; i++)
	{
		// Palette contains ARGB colors.
		uint8_t r8 = m_palette[i * 4 + 1];
		uint8_t g8 = m_palette[i * 4 + 2];
		uint8_t b8 = m_palette[i * 4 + 3];

		// Convert the RGB888 color to an RGB332 color.
		// The RGB332 value is also the index for this color in the standard
		// palette. The pixels having palette index i will be updated with this
		// new palette index which points to the new location of the converted
		// RGB888 color that was originally stored at index i.

		if (color_mode == COLORMODE_DISTANCE)
		{
			m_std_palette_index[i] = rgb888_to_rgb332(get_nearest_color(RGB888(r8, g8, b8), true), COLORMODE_ROUND);
		}
		else
		{
			m_std_palette_index[i] = rgb888_to_rgb332(RGB888(r8, g8, b8), color_mode);
		}

		// Create the standard RGB332/RGB333 color for this palette index.
		// The standard RGB332 color has the same value as its index in the
		// standard palette. The actual color displayed on the Spectrum Next
		// is an RGB333 color where the lowest blue bit as a bitwise OR between
		// the two blue bits in the RGB332 color.

		// Convert the standard RGB333 color back to an RGB888 color.
		uint32_t rgb888 = rgb332_to_rgb888(i);

		// Update the palette with the RGB888 representation of the standard RGB333 color.
		m_palette[i * 4 + 0] = 0;
		m_palette[i * 4 + 1] = rgb888 >> 16;
		m_palette[i * 4 + 2] = rgb888 >> 8;
		m_palette[i * 4 + 3] = rgb888;
	}
}

static void create_sms_palette(color_mode_t color_mode)
{
	// Create the SMS palette.
	// The RGB888 colors in the BMP palette are converted to BRG222 colors.
	for (int i = 0; i < 16; i++)
	{
		// Palette contains ARGB colors.
		uint8_t r8 = m_palette[i * 4 + 1];
		uint8_t g8 = m_palette[i * 4 + 2];
		uint8_t b8 = m_palette[i * 4 + 3];
		uint8_t bgr222 = rgb888_to_bgr222(RGB888(r8, g8, b8), color_mode);

		((uint8_t *) m_next_palette)[i] = bgr222;
	}
}

static void create_next_palette(color_mode_t color_mode)
{
	// Create the next palette.
	// The RGB888 colors in the BMP palette are converted to RGB333 colors,
	// which are then split in RGB332 and B1 parts.
	uint32_t palette_count = m_args.colors_4bit && !m_args.pal_full ? 16 : 256;

	if (m_args.colors_1bit)
	{
		m_next_palette[0] = 0x0000;		// Black
		for (int i = 1; i < palette_count; i++)
			m_next_palette[i] = 0x01ff;	// White
	}
	else
	{
		for (int i = 0; i < palette_count; i++)
		{
			// Palette contains ARGB colors.
			uint8_t r8 = m_palette[i * 4 + 1];
			uint8_t g8 = m_palette[i * 4 + 2];
			uint8_t b8 = m_palette[i * 4 + 3];
			uint16_t rgb333 = rgb888_to_rgb333(RGB888(r8, g8, b8), color_mode);
			uint8_t rgb332 = (uint8_t) (rgb333 >> 1);
			uint8_t b1 = (uint8_t) (rgb333 & 1);

			// Access as bytes for 8-bit palette
			if (m_args.pal_rgb332)
			{
				((uint8_t *) m_next_palette)[i] = rgb332;
			}
			else
			{
				m_next_palette[i] = (b1 << 8) | rgb332;
			}
		}
	}
} 

static int compare_color(const void *p1, const void *p2)
{
	uint8_t *color1 = (uint8_t *) p1;
	uint8_t *color2 = (uint8_t *) p2;

	uint8_t r1 = color1[1];
	uint8_t g1 = color1[2];
	uint8_t b1 = color1[3];

	uint8_t r2 = color2[1];
	uint8_t g2 = color2[2];
	uint8_t b2 = color2[3];

	uint32_t rgb1 = RGB888(r1, g1, b1);
	uint32_t rgb2 = RGB888(r2, g2, b2);

	return (rgb1 > rgb2) ? 1 : (rgb1 < rgb2) ? -1 : 0;
}

static int create_minimized_palette(void)
{
	uint32_t *min_palette_colors = (uint32_t *) m_min_palette;
	int last_unique_color_index = 0;

	memcpy(m_min_palette, m_palette, sizeof(m_palette));

	// Sort the palette colors in ascending RGB order.
	qsort(m_min_palette, NUM_PALETTE_COLORS, sizeof(uint32_t), compare_color);

	// Remove any duplicated palette colors.
	for (int i = 0; i < NUM_PALETTE_COLORS; i++)
	{
		if (min_palette_colors[i] != min_palette_colors[last_unique_color_index])
		{
			min_palette_colors[++last_unique_color_index] = min_palette_colors[i];
		}
	}

	// Set any unused palette entries to 0 (black).
	for (int i = last_unique_color_index + 1; i < NUM_PALETTE_COLORS; i++)
	{
		min_palette_colors[i] = 0;
	}

	// Return number of unique palette colors.
	return last_unique_color_index + 1;
}

static void create_minimized_palette_index_table(void)
{
	uint32_t *palette_colors = (uint32_t *) m_palette;
	uint32_t *min_palette_colors = (uint32_t *) m_min_palette;

	/*
	* Iterate over the originally converted palette and for each color, look up
	* its new index in the minimized palette and write that index in the index
	* table at the same position as the color in the originally converted
	* palette. This index table will be used to update the pixels in the image
	* to use the minimized palette.
	*/

	for (int i = 0; i < NUM_PALETTE_COLORS; i++)
	{
		for (int j = 0; j < NUM_PALETTE_COLORS; j++)
		{
			if (palette_colors[i] == min_palette_colors[j])
			{
				m_min_palette_index[i] = j;
				break;
			}
		}
	}
}

static void shrink_to_4bit_palette(void)
{
	uint32_t *palette_colors = (uint32_t *) m_palette;

	// Set palette entries 16 to 255 to color 0 (black).
	for (int i = 16; i < NUM_PALETTE_COLORS; i++)
	{
		palette_colors[i] = 0;
	}
}

static void print_usage(void)
{
	printf("gfx2next v%s\n", VERSION);
	printf("Converts an uncompressed 8-bit BMP or PNG file to the Sinclair ZX Spectrum Next graphics format(s).\n");
	printf("Usage:\n");
	printf("  gfx2next [options] <srcfile> [<dstfile>]\n");
	printf("\n");
	printf("Options:\n");
	printf("  -debug                  Output additional debug information\n");
	printf("  -font                   Sets output to Next font format (.spr)\n");
	printf("  -screen                 Sets output to Spectrum screen format (.scr)\n");
	printf("  -screen-noattribs       Remove color attributes\n");
	printf("  -bitmap                 Sets output to Next bitmap mode (.nxi)\n");
	printf("  -bitmap-y               Get bitmap in Y order first. (Default is X order first)\n");
	printf("  -bitmap-size=XxY        Splits up the bitmap output file into X x Y sections\n");
	printf("  -sprites                Sets output to Next sprite mode (.spr)\n");
	printf("  -tiles-file=<filename>  Load tiles from file in .nxt format\n");
	printf("  -tile-size=XxY          Sets tile size to X x Y\n");
	printf("  -tile-norepeat          Remove repeating tiles\n");
	printf("  -tile-nomirror          Remove repeating and mirrored tiles\n");
	printf("  -tile-norotate          Remove repeating, rotating and mirrored tiles\n");
	printf("  -tile-y                 Get tile in Y order first. (Default is X order first)\n");
	printf("  -tile-ldws              Get tile in Y order first for ldws instruction. (Default is X order first)\n");
	printf("  -tile-offset=n          Sets the starting tile offset to n tiles\n");
	printf("  -tile-offset-auto       Adds tile offset when using wildcards\n");
	printf("  -tile-pal=n             Sets the palette offset attribute to n\n");
	printf("  -tile-pal-auto          Increments palette offset when using wildcards\n");
	printf("  -tile-none              Don't save a tile file\n");
	printf("  -tile-planar4           Output tiles in planar (4 planes) rather than chunky format\n");
	printf("  -tiled                  Process file(s) in .tmx format\n");
	printf("  -tiled-tsx              Outputs the tileset data as a separate .tsx file\n");
	printf("  -tiled-file=<filename>  Load map from file in .tmx format\n");
	printf("  -tiled-blank=n          Set the tile id of the blank tile\n");
	printf("  -tiled-output           Outputs tile and map data to Tiled .tmx and .tsx format\n");
	printf("  -tiled-width=n          Sets Tiled tileset width output in pixels (default is 256)\n");
	printf("  -block-size=XxY         Sets blocks size to X x Y for blocks of tiles\n");
	printf("  -block-size=n           Sets blocks size to n bytes for blocks of tiles\n");
	printf("  -block-norepeat         Remove repeating blocks\n");
	printf("  -block-16bit            Get blocks as 16 bit index for < 256 blocks\n");
	printf("  -map-none               Don't save a map file\n");
	printf("  -map-16bit              Save map as 16 bit output\n");
	printf("  -map-y                  Save map in Y order first. (Default is X order first)\n");
	printf("  -map-sms                Save 16-bit map with Sega Master System attribute format\n");
	printf("  -bank-8k                Splits up output file into multiple 8k files\n");
	printf("  -bank-16k               Splits up output file into multiple 16k files\n");
	printf("  -bank-48k               Splits up output file into multiple 48k files\n");
	printf("  -bank-sections=name,... Section names for asm files\n");
	printf("  -color-distance         Use the shortest distance between color values (default)\n");
	printf("  -color-floor            Round down the color values to the nearest integer\n");
	printf("  -color-ceil             Round up the color values to the nearest integer\n");
	printf("  -color-round            Round the color values to the nearest integer\n");
	printf("  -colors-4bit            Use 4 bits per pixel (16 colors). Default is 8 bits per pixel (256 colors)\n");
	printf("                          Get sprites or tiles as 16 colors, top 4 bits of 16 bit map is palette index\n");
	printf("  -colors-1bit            Use 1 bits per pixel (2 colors). Default is 8 bits per pixel (256 colors)\n");
	printf("  -pal-file=<filename>    Load palette from file in .nxp format\n");
	printf("  -pal-embed              The raw palette is prepended to the raw image file\n");
	printf("  -pal-ext                The raw palette is written to an external file (.nxp). This is the default\n");
	printf("  -pal-min                If specified, minimize the palette by removing any duplicated colors, sort\n");
	printf("                          it in ascending order, and clear any unused palette entries at the end\n");
	printf("                          This option is ignored if the -pal-std option is given\n");
	printf("  -pal-full               Generate the full palette for -colors-4bit mode\n");
	printf("  -pal-std                If specified, convert to the Spectrum Next standard palette colors\n");
	printf("                          This option is ignored if the -colors-4bit option is given\n");
	printf("  -pal-none               No raw palette is created\n");
	printf("  -pal-rgb332             Output palette in RGB332 (8-bit) format\n");
	printf("  -pal-bgr222             Output palette in BGR222 (8-bit) format. Bits 7-6 are unused\n");
	printf("  -zx0                    Compress all data using zx0\n");
	printf("  -zx0-screen             Compress screen data using zx0\n");
	printf("  -zx0-bitmap             Compress bitmap data using zx0\n");
	printf("  -zx0-sprites            Compress sprite data using zx0\n");
	printf("  -zx0-tiles              Compress tile data using zx0\n");
	printf("  -zx0-blocks             Compress block data using zx0\n");
	printf("  -zx0-map                Compress map data using zx0\n");
	printf("  -zx0-palette            Compress palette data using zx0\n");
	printf("  -zx0-back               Set zx0 to reverse compression mode\n");
	printf("  -zx0-quick              Set zx0 to quick compression mode\n");
	printf("  -asm-z80asm             Generate header and asm binary include files (in Z80ASM format)\n");
	printf("  -asm-sjasm              Generate asm binary incbin file (SjASM format)\n");
	printf("  -asm-file=<name>        Append asm and header output to <name>.asm and <name>.h\n");
	printf("  -asm-start              Specifies the start of the asm and header data for appending\n");
	printf("  -asm-start-auto         Sets start parameter for first item when using wildcards\n");
	printf("  -asm-end                Specifies the end of the asm and header data for appending\n");
	printf("  -asm-end-auto           Sets end parameter for first item when using wildcards\n");
	printf("  -asm-sequence           Add sequence section for multi-bank spanning data\n");
	printf("  -preview                Generate png preview file(s)\n");
}

static bool parse_args(int argc, char *argv[], arguments_t *args)
{
	if (argc == 1)
	{
		print_usage();
		return false;
	}

	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (!strcmp(argv[i], "-debug"))
			{
				m_args.debug = true;
			}
			else if (!strcmp(argv[i], "-font"))
			{
				m_args.font = true;
				m_args.pal_mode = PALMODE_NONE;
			}
			else if (!strcmp(argv[i], "-screen"))
			{
				m_args.screen = true;
				m_args.pal_mode = PALMODE_NONE;
			}
			else if (!strcmp(argv[i], "-screen-attribs"))
			{
				m_args.screen_attribs = true;
			}
			else if (!strcmp(argv[i], "-bitmap"))
			{
				m_args.bitmap = true;
				m_args.map_none = true;
			}
			else if (!strcmp(argv[i], "-bitmap-y"))
			{
				m_args.bitmap = true;
				m_args.bitmap_y = true;
			}
			else if (!strncmp(argv[i], "-bitmap-size=", 13))
			{
				m_bitmap_width = atoi(strtok(&argv[i][13], "x"));
				m_bitmap_height = atoi(strtok(NULL, "x"));
				
				printf("Bitmap Size = %d x %d\n", m_bitmap_width, m_bitmap_height);
			}
			else if (!strcmp(argv[i], "-sprites"))
			{
				m_args.map_none = true;
				m_tile_width = 16;
				m_tile_height = 16;
				m_tile_size = m_tile_width * m_tile_height;
				m_args.tile_norepeat = false;
				m_args.tile_norotate = false;
				m_args.sprites = true;
			}
			else if (!strncmp(argv[i], "-tiles-file=", 12))
			{
				m_args.tiles_file = &argv[i][12];
			}
			else if (!strncmp(argv[i], "-tile-size=", 11))
			{
				m_tile_width = atoi(strtok(&argv[i][11], "x"));
				m_tile_height = atoi(strtok(NULL, "x"));
				m_tile_size = m_tile_width * m_tile_height;
				
				printf("Tile Size = %d x %d\n", m_tile_width, m_tile_height);
			}
			else if (!strcmp(argv[i], "-tile-norepeat"))
			{
				m_args.tile_norepeat = true;
			}
			else if (!strcmp(argv[i], "-tile-nomirror"))
			{
				m_args.tile_nomirror = true;
			}
			else if (!strcmp(argv[i], "-tile-norotate"))
			{
				m_args.tile_norotate = true;
			}
			else if (!strcmp(argv[i], "-tile-y"))
			{
				m_args.tile_y = true;
			}
			else if (!strcmp(argv[i], "-tile-ldws"))
			{
				m_args.tile_ldws = true;
			}
			else if (!strncmp(argv[i], "-tile-offset=", 13))
			{
				m_args.tile_offset = atoi(&argv[i][13]);
			}
			else if (!strcmp(argv[i], "-tile-offset-auto"))
			{
				m_args.tile_offset_auto = true;
			}
			else if (!strncmp(argv[i], "-tile-pal=", 10))
			{
				m_args.tile_pal = atoi(&argv[i][10]);
			}
			else if (!strcmp(argv[i], "-tile-pal-auto"))
			{
				m_args.tile_pal_auto = true;
			}
			else if (!strcmp(argv[i], "-tile-none"))
			{
				m_args.tile_none = true;
			}
			else if (!strcmp(argv[i], "-tile-planar4"))
			{
				m_args.tile_planar4 = true;
				m_args.colors_4bit = true;
			}
			else if (!strcmp(argv[i], "-tiled"))
			{
				m_args.tiled = true;
			}
			else if (!strcmp(argv[i], "-tiled-tsx"))
			{
				m_args.tiled_tsx = true;
			}
			else if (!strncmp(argv[i], "-tiled-file=", 12))
			{
				m_args.tiled_file = &argv[i][12];
			}
			else if (!strncmp(argv[i], "-tiled-blank=", 13))
			{
				m_args.tiled_blank = atoi(&argv[i][13]);

				printf("Tiled Blank = %d\n", m_args.tiled_blank);
			}
			else if (!strcmp(argv[i], "-tiled-output"))
			{
				m_args.tiled_output = true;
			}
			else if (!strncmp(argv[i], "-tiled-width=", 13))
			{
				m_args.tiled_width = atoi(&argv[i][13]);

				printf("Tiled Width = %d\n", m_args.tiled_width);
			}
			else if (!strncmp(argv[i], "-block-size=", 12))
			{
				m_block_width = atoi(strtok(&argv[i][12], "x"));
				m_block_height = atoi(strtok(NULL, "x"));
				m_block_size = m_block_width * m_block_height;
				
				printf("Block Size = %d x %d\n", m_block_width, m_block_height);
			}
			else if (!strcmp(argv[i], "-block-norepeat"))
			{
				m_args.block_norepeat = true;
			}
			else if (!strcmp(argv[i], "-block-16bit"))
			{
				m_args.block_16bit = true;
			}
			else if (!strcmp(argv[i], "-map-none"))
			{
				m_args.map_none = true;
			}
			else if (!strcmp(argv[i], "-map-16bit"))
			{
				m_args.map_16bit = true;
			}
			else if (!strcmp(argv[i], "-map-y"))
			{
				m_args.map_y = true;
			}
			else if (!strcmp(argv[i], "-map-sms"))
			{
				m_args.map_sms = true;
				m_args.map_16bit = true;
				m_args.map_y = false;
			}
			else if (!strcmp(argv[i], "-bank-8k"))
			{
				m_args.bank_size = BANKSIZE_8K;
				m_bank_size = SIZE_8K;
				printf("Bank Size = %d\n", m_bank_size);
			}
			else if (!strcmp(argv[i], "-bank-16k"))
			{
				m_args.bank_size = BANKSIZE_16K;
				m_bank_size = SIZE_16K;
				printf("Bank Size = %d\n", m_bank_size);
			}
			else if (!strcmp(argv[i], "-bank-48k"))
			{
				m_args.bank_size = BANKSIZE_48K;
				m_bank_size = SIZE_48K;
				printf("Bank Size = %d\n", m_bank_size);
			}
			else if (!strncmp(argv[i], "-bank-size=", 11))
			{
				m_args.bank_size = BANKSIZE_CUSTOM;
				m_bank_size = atoi(&argv[i][11]);
				printf("Bank Size = %d\n", m_bank_size);
			}
			else if (!strncmp(argv[i], "-bank-sections=", 15))
			{
				char *token = strtok(&argv[i][15], ",");

				while (token != NULL)
				{
					strcpy(m_bank_sections[m_bank_section_count++], token);
					token = strtok(NULL, ",");
				}
			}
			else if (!strncmp(argv[i], "-bank-used=", 11))
			{
				char *token = strtok(&argv[i][11], ",");

				while (token != NULL)
				{
					if (strncmp("BANK_", token, 5) == 0)
					{
						uint8_t bank_index =  atoi(&token[5]);
						m_bank_used[bank_index] = atoi(strchr(token, '=') + 1);
					}
					token = strtok(NULL, ",");
				}
			}
			else if (!strcmp(argv[i], "-color-distance"))
			{
				m_args.color_mode = COLORMODE_DISTANCE;
			}
			else if (!strcmp(argv[i], "-color-floor"))
			{
				m_args.color_mode = COLORMODE_FLOOR;
			}
			else if (!strcmp(argv[i], "-color-ceil"))
			{
				m_args.color_mode = COLORMODE_CEIL;
			}
			else if (!strcmp(argv[i], "-color-round"))
			{
				m_args.color_mode = COLORMODE_ROUND;
			}
			else if (!strcmp(argv[i], "-colors-4bit"))
			{
				m_args.colors_1bit = false;
				m_args.colors_4bit = true;
			}
			else if (!strcmp(argv[i], "-colors-1bit"))
			{
				m_args.colors_4bit = false;
				m_args.colors_1bit = true;
			}
			else if (!strncmp(argv[i], "-pal-file=", 10))
			{
				m_args.pal_file = &argv[i][10];
			}
			else if (!strcmp(argv[i], "-pal-embed"))
			{
				m_args.pal_mode = PALMODE_EMBEDDED;
			}
			else if (!strcmp(argv[i], "-pal-ext"))
			{
				m_args.pal_mode = PALMODE_EXTERNAL;
			}
			else if (!strcmp(argv[i], "-pal-min"))
			{
				m_args.pal_min = true;
			}
			else if (!strcmp(argv[i], "-pal-std"))
			{
				m_args.pal_std = true;
			}
			else if (!strcmp(argv[i], "-pal-full"))
			{
				m_args.pal_full = true;
			}
			else if (!strcmp(argv[i], "-pal-none"))
			{
				m_args.pal_mode = PALMODE_NONE;
			}
			else if (!strcmp(argv[i], "-pal-rgb332"))
			{
				m_args.pal_rgb332 = true;
			}
			else if (!strcmp(argv[i], "-pal-bgr222"))
			{
				m_args.pal_bgr222 = true;
			}
			else if (!strcmp(argv[i], "-zx0"))
			{
				m_args.compress = COMPRESS_ALL;
			}
			else if (!strcmp(argv[i], "-zx0-screen"))
			{
				m_args.compress |= COMPRESS_SCREEN;
			}
			else if (!strcmp(argv[i], "-zx0-bitmap"))
			{
				m_args.compress |= COMPRESS_BITMAP;
			}
			else if (!strcmp(argv[i], "-zx0-sprites"))
			{
				m_args.compress |= COMPRESS_SPRITES;
			}
			else if (!strcmp(argv[i], "-zx0-tiles"))
			{
				m_args.compress |= COMPRESS_TILES;
			}
			else if (!strcmp(argv[i], "-zx0-blocks"))
			{
				m_args.compress |= COMPRESS_BLOCKS;
			}
			else if (!strcmp(argv[i], "-zx0-map"))
			{
				m_args.compress |= COMPRESS_MAP;
			}
			else if (!strcmp(argv[i], "-zx0-palette"))
			{
				m_args.compress |= COMPRESS_PALETTE;
			}
			else if (!strcmp(argv[i], "-zx0-back"))
			{
				m_args.zx0_back = true;
			}
			else if (!strcmp(argv[i], "-zx0-quick"))
			{
				m_args.zx0_quick = true;
			}
			else if (!strcmp(argv[i], "-asm-z80asm") || !strcmp(argv[i], "-z80asm"))
			{
				m_args.asm_mode = ASMMODE_Z80ASM;
			}
			else if (!strcmp(argv[i], "-asm-sjasm") || !strcmp(argv[i], "-sjasm"))
			{
				m_args.asm_mode = ASMMODE_SJASM;
			}
			else if (!strncmp(argv[i], "-asm-file=", 10))
			{
				m_args.asm_file = &argv[i][10];
			}
			else if (!strcmp(argv[i], "-asm-start"))
			{
				m_args.asm_start = true;
			}
			else if (!strcmp(argv[i], "-asm-start-auto"))
			{
				m_args.asm_start_auto = true;
			}
			else if (!strcmp(argv[i], "-asm-end"))
			{
				m_args.asm_end = true;
			}
			else if (!strcmp(argv[i], "-asm-end-auto"))
			{
				m_args.asm_end_auto = true;
			}
			else if (!strcmp(argv[i], "-asm-sequence"))
			{
				m_args.asm_sequence = true;
			}
			else if (!strcmp(argv[i], "-preview"))
			{
				m_args.preview = true;
			}
			else if (!strcmp(argv[i], "-easter-egg"))
			{
				write_easter_egg();
				exit(0);
				return false;
			}
			else if (!strcmp(argv[i], "-help"))
			{
				print_usage();
				return false;
			}
			else
			{
				fprintf(stderr, "Invalid option: %s\n", argv[i]);
				print_usage();
				return false;
			}
		}
		else
		{
			if (m_args.in_filename == NULL)
			{
				m_args.in_filename = argv[i];
			}
			else if (m_args.out_filename == NULL)
			{
				m_args.out_filename = argv[i];
			}
			else
			{
				fprintf(stderr, "Too many arguments.\n");
				print_usage();
				return false;
			}
		}
	}

	if (m_args.in_filename == NULL)
	{
		fprintf(stderr, "Input file not specified.\n");
		print_usage();
		return false;
	}
	
	if (m_args.out_filename == NULL)
	{
		m_args.out_filename = m_args.in_filename;
	}

	return true;
}

static void create_name(char *out_name, const char *in_filename)
{
	char *start = strrchr(in_filename, DIR_SEPERATOR_CHAR);
	
	strcpy(out_name, start == NULL ? in_filename : start + 1);

	char *end = strchr(out_name, '.');
	end = (end == NULL ? out_name + strlen(out_name) : end);
	
	strcpy(end, "");
}

static void create_filename(char *out_filename, const char *in_filename, const char *extension, bool use_compression)
{
	char *start = strrchr(in_filename, DIR_SEPERATOR_CHAR);
	
	strcpy(out_filename, start == NULL ? in_filename : start + 1);

	char *end = strrchr(out_filename, '.');
	end = (end == NULL ? out_filename + strlen(out_filename) : end);

	strcpy(end, extension);
	
	if (use_compression)
		strcat(out_filename, EXT_ZX0);
}

static void create_series_filename(char *out_filename, const char *in_filename, const char *extension, bool use_compression, int index)
{
	char temp_filename[256] = { 0 };
	char *start = strrchr(in_filename, DIR_SEPERATOR_CHAR);
	
	strcpy(out_filename, start == NULL ? in_filename : start + 1);
	
	char *end = strchr(out_filename, '.');
	out_filename[end == NULL ? strlen(out_filename) : (int)(end - out_filename)] = '\0';

	snprintf(temp_filename, 255, "%s_%d%s", out_filename, index, extension);
	
	strcpy(out_filename, temp_filename);

	if (use_compression)
		strcat(out_filename, EXT_ZX0);
}

static void to_upper(char *filename)
{
	int i = 0;
	while(m_bitmap_filename[i])
	{
		filename[i] = toupper((int) filename[i]);
		i++;
	}
}

static void alphanumeric_to_underscore(char *filename)
{
	int len = strlen(filename);
	
	for (int i = 0; i < len; i++)
	{
		if ((filename[i] == 95) ||
			(filename[i] >= 48 && filename[i] <= 57) ||
			(filename[i] >= 65 && filename[i] <= 90) ||
			(filename[i] >= 97 && filename[i] <= 122))
			continue;
		
		filename[i] = '_';
	}
}

static bool is_valid_bmp_file(uint32_t *palette_offset,
								uint32_t *image_offset,
								uint16_t *bpp)
{
	if ((m_bmp_header[0] != 'B') || (m_bmp_header[1] != 'M'))
	{
		fprintf(stderr, "Not a BMP file.\n");
		return false;
	}

	uint32_t file_size = *((uint32_t *) (m_bmp_header + 2));
	if (file_size < BMP_MIN_FILE_SIZE)
	{
		fprintf(stderr, "Invalid size of BMP file.\n");
		return false;
	}

	*image_offset = *((uint32_t *) (m_bmp_header + 10));
	if (*image_offset >= file_size)
	{
		fprintf(stderr, "Invalid header of BMP file.\n");
		return false;
	}

	uint32_t dib_header_size = *((uint32_t *) (m_bmp_header + 14));
	if (dib_header_size < BMP_MIN_DIB_HEADER_SIZE)
	{
		// At least a BITMAPINFOHEADER is required.
		fprintf(stderr, "Invalid/unsupported header of BMP file.\n");
		return false;
	}

	*palette_offset = BMP_FILE_HEADER_SIZE + dib_header_size;

	m_image_width = *((uint32_t *) (m_bmp_header + 18));
	if (m_image_width == 0)
	{
		fprintf(stderr, "Invalid image width in BMP file.\n");
		return false;
	}

	m_image_height = *((int32_t *) (m_bmp_header + 22));
	if (m_image_height == 0)
	{
		fprintf(stderr, "Invalid image height in BMP file.\n");
		return false;
	}
	
	*bpp = *((uint16_t *) (m_bmp_header + 28));
	if (*bpp != 4 && *bpp != 8)
	{
		fprintf(stderr, "Not a 4-bit or 8-bit BMP file.\n");
		return false;
	}
	
	uint32_t image_size = m_image_width * abs(m_image_height);
	
	if (*bpp == 4)
		image_size >>= 1;

	if (image_size >= file_size)
	{
		fprintf(stderr, "Invalid image size in BMP file.\n");
		return false;
	}

	uint32_t compression = *((uint32_t *) (m_bmp_header + 30));
	if (compression != 0)
	{
		fprintf(stderr, "Not an uncompressed BMP file.\n");
		return false;
	}

	return true;
}

static void read_bitmap()
{
	uint32_t palette_offset;
	uint32_t image_offset;
	uint16_t bpp;
	uint32_t image_size;
	
	// Open the BMP file and validate its header.
	FILE *in_file = fopen(m_args.in_filename, "rb");
	if (in_file == NULL)
	{
		exit_with_msg("Can't open file %s.\n", m_args.in_filename);
	}
	if (fread(m_bmp_header, sizeof(uint8_t), sizeof(m_bmp_header), in_file) != sizeof(m_bmp_header))
	{
		exit_with_msg("Can't read the BMP header in file %s.\n", m_args.in_filename);
	}
	if (!is_valid_bmp_file(&palette_offset, &image_offset, &bpp))
	{
		exit_with_msg("The file %s is not a valid or supported BMP file.\n", m_args.in_filename);
	}

	// Allocate memory for image data.
	// Note: Image width is padded to a multiple of 4 bytes.
	m_bottom_to_top_image = (m_image_height > 0);
	m_padded_image_width = (m_image_width + 3) & ~0x03;
	m_image_height = m_bottom_to_top_image ? m_image_height : -m_image_height;
	m_image_size = m_padded_image_width * m_image_height;
	m_image = malloc(m_image_size);
	
	image_size = (bpp == 4 ? m_image_size >> 1 : m_image_size);
	
	if (m_image == NULL)
	{
		exit_with_msg("Can't allocate memory for image data.\n");
	}

	// Read the palette and image data.
	if (fseek(in_file, palette_offset, SEEK_SET) != 0)
	{
		exit_with_msg("Can't access the BMP palette in file %s.\n", m_args.in_filename);
	}
	if (fread(m_palette, sizeof(uint8_t), sizeof(m_palette), in_file) != sizeof(m_palette))
	{
		exit_with_msg("Can't read the BMP palette in file %s.\n", m_args.in_filename);
	}
	if (fseek(in_file, image_offset, SEEK_SET) != 0)
	{
		exit_with_msg("Can't access the BMP image data in file %s.\n", m_args.in_filename);
	}
	if (fread(m_image, sizeof(uint8_t), image_size, in_file) != image_size)
	{
		exit_with_msg("Can't read the BMP image data in file %s.\n", m_args.in_filename);
	}
	
	// Convert 4-bit to 8-bit data
	if (bpp == 4)
	{
		for (int i = m_image_size-2; i >= 0; i--)
		{
			uint8_t value = m_image[i >> 1];
			
			m_image[i] = (i & 1 ? value & 0xf : value >> 4);
		}
	}
	
	uint32_t num_palette_colors = (bpp == 4 ? 16 : 256);
	
	for (int i = 0; i < num_palette_colors; i++)
	{
		// BGRA to ARGB
		uint8_t b8 = m_palette[i * 4 + 0];
		uint8_t g8 = m_palette[i * 4 + 1];
		uint8_t r8 = m_palette[i * 4 + 2];
		uint8_t a8 = m_palette[i * 4 + 3];
		
		m_palette[i * 4 + 0] = a8;
		m_palette[i * 4 + 1] = r8;
		m_palette[i * 4 + 2] = g8;
		m_palette[i * 4 + 3] = b8;
	}
	
	fclose(in_file);
}

static void read_png()
{
	unsigned error;
	unsigned char *image = 0;
	unsigned width, height;
	unsigned char *png = 0;
	size_t pngsize;
	LodePNGState state;

	lodepng_state_init(&state);
	
	state.decoder.color_convert = false;
	state.info_raw.colortype = LCT_PALETTE;
	state.info_raw.bitdepth = 8;

	error = lodepng_load_file(&png, &pngsize, m_args.in_filename);
	
	if (error)
	{
		exit_with_msg("Can't read the Png image data in file %s.\n", m_args.in_filename);
	}
	
	error = lodepng_decode(&image, &width, &height, &state, png, pngsize);
	
	free(png);
	
	if(error)
	{
		exit_with_msg("Can't read the Png image data in file %s (error %u: %s).\n", m_args.in_filename, error, lodepng_error_text(error));
	}
	
	if (state.info_png.color.colortype != LCT_PALETTE || state.info_png.color.bitdepth != 8)
	{
		exit_with_msg("Can't read the Png image format. Must be a paletted 8-bit image.\n");
	}
	
	m_image_width = width;
	m_image_height = height;
	m_padded_image_width = m_image_width;
	m_image_size = m_padded_image_width * m_image_height;
	
	//printf("width: %d height: %d size: %d pngsize: %d palettesize: %d bitdepth: %d\n", m_image_width, m_image_height, m_image_size, pngsize, state.info_png.color.palettesize, state.info_png.color.bitdepth);
	
	m_image = malloc(m_image_size);
	
	if (m_image == NULL)
	{
		exit_with_msg("Can't allocate memory for image data.\n");
	}
	
	memcpy(m_palette, state.info_png.color.palette, state.info_png.color.palettesize * 4);
	
	for (int i = 0; i < NUM_PALETTE_COLORS; i++)
	{
		// RGBA to ARGB
		uint8_t r8 = m_palette[i * 4 + 0];
		uint8_t g8 = m_palette[i * 4 + 1];
		uint8_t b8 = m_palette[i * 4 + 2];
		uint8_t a8 = m_palette[i * 4 + 3];
		
		m_palette[i * 4 + 0] = a8;
		m_palette[i * 4 + 1] = r8;
		m_palette[i * 4 + 2] = g8;
		m_palette[i * 4 + 3] = b8;
	}
	
	memcpy(m_image, image, m_image_size);
	
	lodepng_state_cleanup(&state);
	free(image);
}

static void write_png_bits(const char *in_filename, uint8_t *p_image, int width, int height, bool is_4bit)
{
	unsigned error;
	unsigned char *image = NULL;
	size_t outsize;
	LodePNGState state;
	
	lodepng_state_init(&state);
	
	uint32_t num_palette_colors = (is_4bit ? 16 : 256);
	
	for (int i = 0; i < num_palette_colors; i++)
	{
		uint32_t rgb888 = rgb332_to_rgb888(m_next_palette[i]);
		uint8_t r8 = rgb888 >> 16;
		uint8_t g8 = rgb888 >> 8;
		uint8_t b8 = rgb888;

		lodepng_palette_add(&state.info_png.color, r8, g8, b8, 0xff);
		lodepng_palette_add(&state.info_raw, r8, g8, b8, 0xff);
	}
	
	state.info_png.color.colortype = LCT_PALETTE;
	state.info_png.color.bitdepth = (is_4bit ? 4 : 8);
	state.info_raw.colortype = LCT_PALETTE;
	state.info_raw.bitdepth = (is_4bit ? 4 : 8);
	state.encoder.auto_convert = 0;
	
	error = lodepng_encode(&image, &outsize, p_image, width, height, &state);
	
	if (error)
	{
		exit_with_msg("Can't write the Png image data in file %s (error %u: %s).\n", in_filename, error, lodepng_error_text(error));
	}
	
	error = lodepng_save_file(image, outsize, in_filename);
	
	if (error)
	{
		exit_with_msg("Can't write the Png image data in file %s (error %u: %s).\n", in_filename, error, lodepng_error_text(error));
	}
	
	lodepng_state_cleanup(&state);
	free(image);
}

static void write_png(const char *in_filename, uint8_t *p_image, int width, int height)
{
	write_png_bits(in_filename, p_image, width, height, m_args.bitmap && m_args.colors_4bit && !m_args.pal_full);
}

static void write_tiles_png(char *png_filename, uint32_t tile_width, uint32_t tile_height, uint32_t tile_offset, uint32_t tile_count, uint32_t tilesheet_width, uint32_t *bitmap_width, uint32_t *bitmap_height)
{
	uint32_t tile_size = tile_width * tile_height;
	uint32_t data_size = tile_count * tile_size;
	*bitmap_width = MIN(tilesheet_width, tile_count * tile_width);
	*bitmap_height = (uint32_t)ceil((double)data_size / *bitmap_width / tile_height) * tile_height;
	uint32_t bitmap_size = *bitmap_width * *bitmap_height;
	uint32_t tile_cols = *bitmap_width / tile_width;
	
	uint8_t *p_image = malloc(bitmap_size);
	
	memset(p_image, 0, bitmap_size);
	
	for (int t = 0; t < tile_count; t++)
	{
		uint32_t tile_id = tile_offset + t;
		uint32_t tile_x = t % tile_cols;
		uint32_t tile_y = t / tile_cols;
		uint32_t src_offset = tile_id * tile_size;
		uint32_t dst_offset = tile_y * *bitmap_width * tile_height + tile_x * tile_width;
		
		for (int y = 0; y < tile_height; y++)
		{
			for (int x = 0; x < tile_width; x++)
			{
				uint32_t src_index = src_offset + y * tile_width + x;
				uint32_t dst_index = dst_offset + y * *bitmap_width + x;

				if (m_args.colors_1bit)
				{
					// Convert back to 8-bit
					// Palette has been fixed so index 0 is black and index 1 is white
					p_image[dst_index] = (m_tiles[src_index>>3] >> (7-(x&0x7))) & 0x01 ? 1 : 0;
				}
				else
				if (m_args.colors_4bit)
				{
					src_index >>= 1;

					if ((x & 1) == 0)
						p_image[dst_index] = m_tiles[src_index] >> 4;
					else
						p_image[dst_index] = m_tiles[src_index] & 0xf;
				}
				else
				{
					p_image[dst_index] = m_tiles[src_index];
				}
			}
		}
	}
	
	write_png_bits(png_filename, p_image, *bitmap_width, *bitmap_height, false);
}

static void process_palette()
{
	// Update the colors in the palette.
	if (m_args.pal_std && !m_args.colors_4bit)
	{
		// Convert the colors in the palette to the Spectrum Next standard palette RGB332 colors.
		convert_standard_palette(m_args.color_mode);

		// Update the image pixels to use the new palette indexes of the standard palette colors.
		for (int i = 0; i < m_image_size; i++)
		{
			m_image[i] = m_std_palette_index[m_image[i]];
		}
	}
	else
	{
		// Convert the colors in the palette to the closest matching RGB333 colors.
		convert_palette(m_args.color_mode);

		if (m_args.pal_min)
		{
			// Minimize the converted palette by removing any duplicated colors and sort it
			// in ascending RGB order. Any unused palette entries at the end are set to 0 (black).
			int num_unique_colors = create_minimized_palette();
			printf("The minimized palette contains %d unique colors.\n", num_unique_colors);

			// Create an index table containing the palette indexes of the minimized palette
			// that correspond to the palette indexes of the originally converted palette.
			create_minimized_palette_index_table();

			// Copy back the minimized palette to the original palette.
			memcpy(m_palette, m_min_palette, sizeof(m_min_palette));

			// Update the image pixels to use the palette indexes of the minimized palette.
			for (int i = 0; i < m_image_size; i++)
			{
				m_image[i] = m_min_palette_index[m_image[i]];
			}

			// Handle 4-bit case.
			if (m_args.colors_4bit && num_unique_colors > 16)
			{
				printf("Warning: The palette contains more than 16 unique colors, %d colors will be discarded.\n",
				num_unique_colors - 16);

				// Shrink the palette to 16 colors.
				shrink_to_4bit_palette();

				// Remove references to discarded colors in image.
				for (int i = 0; i < m_image_size; i++)
				{
					if (m_image[i] > 15)
					{
						m_image[i] = 0;
					}
				}
			}
		}
	}
}

static void read_next_image()
{
	uint8_t *p_image = m_image;
	if (m_bottom_to_top_image)
	{
		p_image += m_image_size - m_padded_image_width;
	}

	// Allocate memory for Next image data.
	m_next_image_width = m_args.bitmap && m_args.colors_4bit ? ((m_image_width + m_image_width % 2) / 2) : m_image_width;
	m_next_image_size = m_next_image_width * m_image_height;
	m_next_image = malloc(m_next_image_size);
	if (m_next_image == NULL)
	{
		exit_with_msg("Can't allocate memory for raw image data.\n");
	}

	if(m_args.debug)
	{
		for(int y=0; y < m_image_height; y++)
		{
			printf("%04d: ", y);
			for(int x = 0; x < m_image_width; x++)
			{
				int pix = p_image[y * m_image_width + x];
				printf("%02x ", pix);
				
				if (x != m_image_width - 1 && (x + 1) % 32 == 0)
					printf("\n      ");
			}
			printf("\n");
		}
	}

	if (m_args.bitmap)
	{
		// Convert the image data to raw image data.
		if (m_args.bitmap_y)
		{
			if (m_args.colors_4bit)
			{
				// 640 x 256 layer 2 mode
				for (int y = 0; y < m_image_height; y++)
				{
					for (int x = 0; x < m_image_width; x += 2)
					{
						uint8_t left_pixel = (p_image[x] & 0x0F) << 4;
						uint8_t right_pixel = p_image[x + 1] & 0x0F;
						m_next_image[y + (x / 2) * m_image_height] = left_pixel | right_pixel;
					}
					p_image = m_bottom_to_top_image ? p_image - m_padded_image_width : p_image + m_padded_image_width;
				}
			}
			else
			{
				// 320 x 256 layer 2 mode
				for (int y = 0; y < m_image_height; y++)
				{
					for (int x = 0; x < m_image_width; x++)
					{
						m_next_image[y + x * m_image_height] = p_image[x];
					}
					p_image = m_bottom_to_top_image ? p_image - m_padded_image_width : p_image + m_padded_image_width;
				}
			}
		}
		else // Row layout
		{
			if (m_args.colors_4bit)
			{
				// 4-bit sprite sheets
				for (int y = 0; y < m_image_height; y++)
				{
					for (int x = 0; x < m_image_width; x += 2)
					{
						uint8_t left_pixel = (p_image[x] & 0x0F) << 4;
						uint8_t right_pixel = p_image[x + 1] & 0x0F;
						m_next_image[y * m_next_image_width + x / 2] = left_pixel | right_pixel;
					}
					p_image = m_bottom_to_top_image ? p_image - m_padded_image_width : p_image + m_padded_image_width;
				}
			}
			else
			{
				// 256 x 192 layer 2 mode and 8-bit sprite sheets
				uint8_t *p_next_image = m_next_image;
				for (int y = 0; y < m_image_height; y++)
				{
					memcpy(p_next_image, p_image, m_image_width);
					p_image = m_bottom_to_top_image ? p_image - m_padded_image_width : p_image + m_padded_image_width;
					p_next_image += m_image_width;
				}
			}
		}
	}
	else
	{
		uint8_t *p_next_image = m_next_image;
		for (int y = 0; y < m_image_height; y++)
		{
			memcpy(p_next_image, p_image, m_image_width);
			p_image = m_bottom_to_top_image ? p_image - m_padded_image_width : p_image + m_padded_image_width;
			p_next_image += m_image_width;
		}
	}
}

static void write_1bit_png(const char *filename, uint8_t *data)
{
	m_args.bitmap = true;
	m_args.colors_4bit = true;
	
	uint8_t buffer_decompress[0x800];
	uint8_t buffer_out[0x2000];
	memset(m_next_palette, 0, sizeof(m_next_palette));
	memset(buffer_out, 0, sizeof(buffer_out));
	
	m_next_palette[1] = 0x01ff;
	
	zx0_decompress(data, buffer_decompress);
	
	uint8_t *p_data = buffer_decompress;
	uint8_t *p_buffer = buffer_out;
	
	for (int i = 0; i < 0x800; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			p_buffer[(i * 8 + j) >> 1] |= p_data[i] & (1 << (7 - j)) ? (j % 2) == 0 ? 0x10 : 1 : 0;
		}
	}
	
	write_png(filename, buffer_out, 128, 128);
}

static void write_easter_egg()
{
	write_1bit_png("gf.png", m_gf);
}

static void write_asm_header()
{
	if (m_args.asm_mode == ASMMODE_SJASM)
	{
		fprintf(m_asm_file, "\tdevice zxspectrum48\n");
	}
	else if (m_args.asm_mode == ASMMODE_Z80ASM)
	{
	}
}

static void write_asm_file(char *p_filename, uint32_t data_size)
{
	char label[256] = { 0 };
	strcpy(label, p_filename);
	alphanumeric_to_underscore(label);
	
	if (m_args.asm_mode == ASMMODE_SJASM)
	{
		if (m_bank_section_index < m_bank_section_count)
			fprintf(m_asm_file, "\norg %s\n", m_bank_sections[m_bank_section_index++]);
		else
			fprintf(m_asm_file, "\norg $c000\n");
		fprintf(m_asm_file, "\nEXPORT %s\n", label);
		fprintf(m_asm_file, "EXPORT %s_end\n", label);
		fprintf(m_asm_file, "\n%s\n", label);
		fprintf(m_asm_file, "\n\tincbin \"binary/%s\"\t; %d bytes\n", p_filename, data_size);
		fprintf(m_asm_file, "\n%s_end\n", label);
	}
	else if (m_args.asm_mode == ASMMODE_Z80ASM)
	{
		if (m_bank_section_index < m_bank_section_count)
		{
			if (strncmp("BANK_", m_bank_sections[m_bank_section_index], 5) == 0)
			{
				m_bank_index = atoi(&m_bank_sections[m_bank_section_index][5]);
				
				m_bank_used[m_bank_index] += data_size;
			}
			
			fprintf(m_asm_file, "\nSECTION %s\n", m_bank_sections[m_bank_section_index++]);
		}
		else
		{
			if (m_bank_index == 0)
			{
				fprintf(m_asm_file, "\nSECTION rodata_user\n");
			}
			else
			{
				if (m_bank_used[m_bank_index] + data_size >= m_bank_size)
				{
					m_bank_index++;
				}
				
				m_bank_used[m_bank_index] += data_size;
				
				fprintf(m_asm_file, "\nSECTION BANK_%d\n", m_bank_index);
			}
		}
		
		fprintf(m_asm_file, "\nPUBLIC _%s\n", label);
		fprintf(m_asm_file, "PUBLIC _%s_end\n", label);
		fprintf(m_asm_file, "\n_%s:\n", label);
		fprintf(m_asm_file, "\n\tBINARY \"binary/%s\"\t; %d bytes\n", p_filename, data_size);
		fprintf(m_asm_file, "\n_%s_end:\n", label);
	}
}

static void write_asm_sequence()
{
	char sequence_filename[256] = { 0 };
	create_filename(sequence_filename, m_args.out_filename, "_sequence", false);
	
	if (m_args.asm_mode == ASMMODE_SJASM)
	{
		if (m_bank_section_index < m_bank_section_count)
			fprintf(m_asm_file, "\norg %s\n", m_bank_sections[m_bank_section_index++]);
		else
			fprintf(m_asm_file, "\norg $c000\n");
		fprintf(m_asm_file, "\nEXPORT %s\n", sequence_filename);
		fprintf(m_asm_file, "\n%s\n", sequence_filename);
		fprintf(m_asm_file, "\tdw ");
		
		for (int i = 0; i < m_bank_count; i++)
		{
			fprintf(m_asm_file, "%s", m_asm_labels[i]);
			
			if (i < m_bank_count-1)
				fprintf(m_asm_file, ",");
		}
	}
	else if (m_args.asm_mode == ASMMODE_Z80ASM)
	{
		if (m_bank_section_index < m_bank_section_count)
			fprintf(m_asm_file, "\nSECTION %s\n", m_bank_sections[m_bank_section_index++]);
		else
			fprintf(m_asm_file, "\nSECTION rodata_user\n");
		fprintf(m_asm_file, "\nPUBLIC _%s\n", sequence_filename);
		fprintf(m_asm_file, "\n_%s:\n", sequence_filename);
		fprintf(m_asm_file, "\tDEFW ");
		
		for (int i = 0; i < m_bank_count; i++)
		{
			fprintf(m_asm_file, "_%s", m_asm_labels[i]);
			
			if (i < m_bank_count-1)
				fprintf(m_asm_file, ",");
		}
	}
}

static void write_header_file(char *p_filename, bool type_16bit)
{
	alphanumeric_to_underscore(p_filename);
	
	fprintf(m_header_file, "extern %s %s[];\n", type_16bit ? "uint16_t" : "uint8_t", p_filename);
	fprintf(m_header_file, "extern uint8_t *%s_end;\n", p_filename);
}

static void write_header_header(char *p_filename)
{
	char header_filename[256] = { 0 };
	create_filename(header_filename, p_filename, "_H", false);
	
	to_upper(header_filename);
	alphanumeric_to_underscore(header_filename);

	fprintf(m_header_file, "#ifndef _%s\n", header_filename);
	fprintf(m_header_file, "#define _%s\n\n", header_filename);
}

static void write_header_footer()
{
	fprintf(m_header_file, "\n#endif\n");
}

static void write_header_sequence()
{	
	char header_filename[256] = { 0 };
	create_filename(header_filename, m_args.out_filename, "_sequence", false);
	
	fprintf(m_header_file, "extern uint8_t *%s;\n", header_filename);
}

static void read_file(char *p_filename, uint8_t *p_buffer, uint32_t buffer_size)
{
	FILE *in_file = fopen(p_filename, "rb");
	if (in_file == NULL)
	{
		exit_with_msg("Can't open file %s.\n", p_filename);
	}
	if (fread(p_buffer, sizeof(uint8_t), buffer_size, in_file) != buffer_size)
	{
		exit_with_msg("Can't read file %s.\n", p_filename);
	}
	
	fclose(in_file);
}

static void write_file(FILE *p_file, char *p_filename, uint8_t *p_buffer, uint32_t buffer_size, bool type_16bit, bool use_compression)
{
	if (use_compression)
	{
		size_t compressed_size = 0;
		
		uint8_t *compressed_buffer = zx0_compress(p_buffer, buffer_size, m_args.zx0_quick, m_args.zx0_back, &compressed_size);

		if (m_args.asm_mode > ASMMODE_NONE)
		{
			write_asm_file(p_filename, compressed_size);
			
			if (m_args.asm_mode == ASMMODE_Z80ASM)
			{
				write_header_file(p_filename, false);
			}
		}
		
		// Write the compressed data to file.
		if (fwrite(compressed_buffer, sizeof(uint8_t), compressed_size, p_file) != compressed_size)
		{
			exit_with_msg("Error writing file %s.\n", p_filename);
		}
		
		free(compressed_buffer);
	}
	else
	{
		if (m_args.asm_mode > ASMMODE_NONE)
		{
			write_asm_file(p_filename, buffer_size);
			
			if (m_args.asm_mode == ASMMODE_Z80ASM)
			{
				write_header_file(p_filename, type_16bit);
			}
		}
		
		// Write the data to file.
		if (fwrite(p_buffer, sizeof(uint8_t), buffer_size, p_file) != buffer_size)
		{
			exit_with_msg("Error writing file %s.\n", p_filename);
		}
	}
}

static void write_next_palette()
{
	// Write the raw palette either prepended to the raw image file or as a separate file.
	uint32_t next_palette_size = m_args.colors_4bit && !m_args.pal_full ? NEXT_4BIT_PALETTE_SIZE : NEXT_PALETTE_SIZE;

	// 8-bit palette is half the regular palette size
	if (m_args.pal_rgb332 || m_args.pal_bgr222)
	{
		next_palette_size /= 2;
	}

	if (m_args.pal_mode == PALMODE_EMBEDDED)
	{
		write_file(m_bitmap_file, m_bitmap_filename, (uint8_t *)m_next_palette, next_palette_size, false, m_args.compress & COMPRESS_PALETTE);
	}
	else if (m_args.pal_mode == PALMODE_EXTERNAL)
	{
		char palette_filename[256] = { 0 };
		
		create_filename(palette_filename, m_bitmap_filename, EXT_NXP, m_args.compress & COMPRESS_PALETTE);
		
		FILE *palette_file = fopen(palette_filename, "wb");
	
		if (palette_file == NULL)
		{
			exit_with_msg("Can't create file %s.\n", palette_filename);
		}
	
		write_file(palette_file, palette_filename, (uint8_t *)m_next_palette, next_palette_size, false, m_args.compress & COMPRESS_PALETTE);
		
		fclose(palette_file);
	}
}

static void write_next_bitmap_file(FILE *bitmap_file, char *bitmap_filename, uint8_t *next_image, uint32_t next_image_size, bool use_compression)
{
	write_file(bitmap_file, bitmap_filename, next_image, next_image_size, false, use_compression);
	
	if (m_args.preview)
	{
		create_filename(m_bitmap_filename, m_args.out_filename, "_preview.png", false);

		write_png(m_bitmap_filename, m_next_image, m_image_width, m_image_height);
	}
}

static uint8_t *get_bitmap_width_height(uint8_t *p_data, int bank_index, int bitmap_width, int bitmap_height, int *bank_size)
{
	static uint8_t bank[0xFFFF];
	int bank_width = bitmap_width;
	int bank_height = m_bank_size / bitmap_width;
	int rows = ceil((float)m_image_height / bank_height);
	int offset_x = (bank_index / rows) * bank_width;
	int offset_y = (bank_index % rows) * bank_height;
	int bank_count = 0;

	memset(bank, 0, sizeof(bank));

	for (int i = 0; i < m_bank_size; i++)
	{
		int x = i / bank_height;
		int y = i % bank_height;
		int image_x = offset_x + x;
		int image_y = offset_y + y;
		int image_offset = image_x + image_y * m_image_width;
		int bank_offset = y * bank_width + x;
		
		if (image_x >= m_image_width || image_y >= m_image_height)
			continue;

		if (image_offset < m_image_size)
			bank[bank_offset] = p_data[image_offset];
		
		bank_count++;
	}
	
	if (bank_count > 0)
	{
		*bank_size = bank_count;
	}

	return bank;
}

static uint8_t *get_bank_width_height(uint8_t *p_data, int bank_index, int bank_width, int bank_height, int bank_size, int *bank_x)
{
	static uint8_t bank[0xFFFF];
	int offset_x = ((bank_index * bank_width) % m_image_width);
	int offset_y = ((bank_index * bank_width) / m_image_width) * bank_height;
	*bank_x = MIN(bank_width, m_image_width - offset_x);
	
	memset(bank, 0, sizeof(bank));
	
	for (int i = 0; i < bank_size; i++)
	{
		int x = i % *bank_x;
		int y = i / *bank_x;
		
		bank[y * bank_width + x] =  p_data[(offset_x + x) + (offset_y + y) * m_image_width];
	}
	
	return bank;
}

static uint8_t *get_bank(uint8_t *p_data, int bank_size)
{
	static uint8_t bank[0xFFFF];

	memset(bank, 0, sizeof(bank));
	
	for (int i = 0; i < bank_size; i++)
	{
		bank[i] = p_data[i];
	}
	
	return bank;
}

/* static void write_1bit()
{
	char onebit_filename[256] = { 0 };
	create_filename(onebit_filename, m_args.out_filename, EXT_BIN, true);
	FILE *p_file = fopen(onebit_filename, "wb");
	
	if (p_file == NULL)
	{
		exit_with_msg("Can't create file %s.\n", onebit_filename);
	}
	
	uint32_t image_size = (m_image_width * m_image_height) / 8;
	uint8_t *p_buffer = malloc(image_size);
	
	for (int y = 0; y < m_image_height; y++)
	{
		for (int x = 0; x < m_image_width; x++)
		{
			int index = y * m_image_width + x;
			p_buffer[index >> 3] |= m_next_image[index] ? 1 << (7 - (x % 8)) : 0;
		}
	}
	
	write_file(p_file, onebit_filename, p_buffer, image_size, false, true);
	
	free(p_buffer);
	fclose(p_file);
} */

static void write_font()
{
	char font_filename[256] = { 0 };
	create_filename(font_filename, m_args.out_filename, EXT_SPR, m_args.compress & COMPRESS_SPRITES);
	FILE *p_file = fopen(font_filename, "wb");
	
	if (p_file == NULL)
	{
		exit_with_msg("Can't create file %s.\n", font_filename);
	}
	
	uint32_t image_size = (m_image_width * m_image_height) / 8;
	uint32_t char_count = image_size / 8;
	uint8_t *p_buffer = malloc(image_size);
	
	for (int i = 0; i < char_count; i++)
	{
		int bank_x;
		uint8_t *p_data = get_bank_width_height(m_next_image, i, 8, 8, 64, &bank_x);
		
		for (int y = 0; y < 8; y++)
		{
			uint8_t data = 0;
			
			for (int x = 0; x < 8; x++)
			{
				if (p_data[y * 8 + x])
					data |= 1 << (7 - x);
			}
			
			p_buffer[i * 8 + y] = data;
		}
	}
	
	write_file(p_file, font_filename, p_buffer, image_size, false, m_args.compress & COMPRESS_SPRITES);
	
	free(p_buffer);
	fclose(p_file);
}

static void write_screen()
{
	char screen_filename[256] = { 0 };
	create_filename(screen_filename, m_args.out_filename, EXT_SCR, m_args.compress & COMPRESS_SCREEN);
	FILE *p_file = fopen(screen_filename, "wb");
	
	if (p_file == NULL)
	{
		exit_with_msg("Can't create file %s.\n", screen_filename);
	}
	
	uint32_t image_size = (m_image_width * m_image_height) / 8;
	uint32_t cols_count = m_image_width / 8;
	uint32_t rows_count = m_image_height / 8;
	uint32_t attrib_size = cols_count * rows_count;
	uint32_t total_size = (m_args.screen_attribs ? image_size : image_size + attrib_size);
	uint8_t *p_buffer = malloc(total_size);
	uint8_t *p_pixels = malloc(image_size);
	uint32_t *p_attrib = malloc(attrib_size * sizeof(uint32_t) * 2);
	
	int pixelCount = 0;
	int attribCount = 0;
	
	memset(p_buffer, 0, total_size);
	
	for (int y = 0; y < m_image_height; y += 8)
	{
		for (int x = 0; x < m_image_width; x += 8)
		{
			int attrCount = 0;
			uint32_t attr[2] = { 0 };
			uint8_t byte[8];
			
			for (int j = 0; j < 8; j++)
			{
				uint8_t row = 0;

				for (int i = 0; i < 8; i++)
				{
					int index = x + i + (j + y) * m_image_width;
					int colorIndex = m_next_image[index];
					uint8_t r8 = m_palette[colorIndex * 4 + 1];
					uint8_t g8 = m_palette[colorIndex * 4 + 2];
					uint8_t b8 = m_palette[colorIndex * 4 + 3];
					uint32_t rgb888 =  RGB888(r8, g8, b8);
					uint32_t color = get_nearest_screen_color(rgb888);
					
					if (attrCount == 0)
						attr[attrCount++] = color;
					
					if (color != attr[0])
						row |= 1 << (7 - i);
					
					bool attrFound = false;
					
					for (int k = 0; k < attrCount; k++)
					{
						if (attr[k] == color)
							attrFound = true;
					}
					
					if (!attrFound)
						attr[attrCount++] = color;
				}
				
				byte[j] = row;
			}
			
			if (attrCount > 2)
				exit_with_msg("More than 2 colors in an attribute block in (%d, %d)\n", x, y);
			else if(attrCount != 2)
			{
				// If only one colour, try to find a match in an adjacent cell
				if (attribCount)
				{
					uint32_t *prevAttr = &p_attrib[attribCount - 2];
					
					if (prevAttr[0] == attr[0])
						attr[attrCount++] = prevAttr[1];
				}
				
				if (attrCount != 2)
					attr[attrCount++] = m_screenColors[0];
			}
			
			// Improve compression ratio
			uint8_t paper = get_screen_color_attribs(attr[0], false);
			uint8_t ink = get_screen_color_attribs(attr[1], true);
			
			if (paper > ink)
			{
				uint32_t attrTemp = attr[0];
				attr[0] = attr[1];
				attr[1] = attrTemp;

				for (int i = 0; i < 8; i++)
					byte[i] = ~byte[i] & 0xff;
			}

			for (int i = 0; i < 8; i++)
				p_pixels[pixelCount++] = byte[i];

			p_attrib[attribCount++] = attr[0];
			p_attrib[attribCount++] = attr[1];
		}
	}
	
	if (!m_args.screen_attribs)
	{
		for (int i = 0; i < attribCount >> 1; i++)
		{
			uint8_t paper = get_screen_color_attribs(p_attrib[i * 2], false);
			uint8_t ink = get_screen_color_attribs(p_attrib[i * 2 + 1], true);

			p_buffer[image_size + i] = (paper | ink);
		}
	}
	
	int pixelIndex = 0;
	
	for (int block = 0; block < 3; block++)
	{
		for (int col = 0; col < 8; col++)
		{
			for (int row = 0; row < 8; row++)
			{
				for (int line = 0; line < 32; line++)
				{
					p_buffer[pixelIndex++] = p_pixels[(block * 8 * 8 * 32) + (row * 32 * 8) + (line * 8) + col];
				}
			}
		}
	}
	
	free(p_pixels);
	free(p_attrib);
	
	write_file(p_file, screen_filename, p_buffer, total_size, false, m_args.compress & COMPRESS_SCREEN);
	
	free(p_buffer);
	fclose(p_file);
}

static void write_next_bitmap()
{
	if (m_args.bank_size > BANKSIZE_NONE)
	{
		int size = m_next_image_size;
		
		m_bank_count = 0;
		
		while (size > 0)
		{
			int bank_size = (size < m_bank_size ? size : m_bank_size);
			
			create_series_filename(m_bitmap_filename, m_args.out_filename, EXT_NXI, m_args.compress & COMPRESS_BITMAP, m_bank_count);

			if (m_args.asm_mode > ASMMODE_NONE)
			{
				strcpy(m_asm_labels[m_bank_count], m_bitmap_filename);
				
				alphanumeric_to_underscore(m_asm_labels[m_bank_count]);
			}

			FILE *bitmap_file = fopen(m_bitmap_filename, "wb");
	
			if (bitmap_file == NULL)
			{
				exit_with_msg("Can't create file %s.\n", m_bitmap_filename);
			}
			
			uint8_t *p_image = NULL;
			
			if (m_bitmap_width != 0 && m_bitmap_height != 0)
				p_image = get_bitmap_width_height(m_next_image, m_bank_count, m_bitmap_width, m_bitmap_height, &bank_size);
			else
				p_image = get_bank(m_next_image + m_bank_count * m_bank_size, bank_size);
			
			write_next_bitmap_file(bitmap_file, m_bitmap_filename, p_image, bank_size, m_args.compress & COMPRESS_BITMAP);
			
			fclose(bitmap_file);

			if (m_args.preview)
			{
				create_series_filename(m_bitmap_filename, m_args.out_filename, "_preview.png", false, m_bank_count);

				if (m_bitmap_width != 0 && m_bitmap_height != 0)
					write_png(m_bitmap_filename, p_image, m_bitmap_width, m_bitmap_height);
				else
					write_png(m_bitmap_filename, p_image, m_image_width, bank_size / m_image_width);
			}
			
			size -= bank_size;
			m_bank_count++;
		}
	}
	else
	{
		write_next_bitmap_file(m_bitmap_file, m_bitmap_filename, m_next_image, m_next_image_size, m_args.compress & COMPRESS_BITMAP);
	}
	
	if (m_args.asm_mode > ASMMODE_NONE)
	{
		if (m_args.asm_sequence && m_bank_count > 0)
		{
			write_asm_sequence();
			
			if (m_args.asm_mode == ASMMODE_Z80ASM)
			{
				write_header_sequence();
			}
		}
	}
}

// Convert 4-bit chunky to planar
void c2p(uint8_t *source, uint32_t size)
{
	uint8_t	planes[4];
	// 4 bytes is 8 pixels
	for (int n=0; n<size; n+=4)
	{
		// 8-pixels at a time
		for (int pixel = 0; pixel < 8; pixel++)
		{
			unsigned char nibble = source[pixel >> 1];

			// Check for upper nibble
			if ((pixel & 1) == 0)
				nibble >>= 4;

			// Planes 0-3
			for (int plane = 0; plane < 4; plane++)
			{
				planes[plane] <<= 1;
				planes[plane] &= 0xfe;
				planes[plane] |= (nibble & 0x01);
				nibble >>= 1;
			}
		}

		// Copy the newly created plane data back
		for (int n=0; n<4; n++)
		{
			source[n] = planes[n];
		}

		source += 4;
	}
}

static void write_tiles_sprites()
{
	char out_filename[256] = { 0 };
	uint32_t tile_offset = 0;
	uint32_t tile_size = (m_args.colors_4bit ? m_tile_size >> 1 : (m_args.colors_1bit ? m_tile_size >> 3 : m_tile_size));
	uint32_t data_size = tile_size * m_tile_count;
	const char *extension = (m_args.sprites ? EXT_SPR : EXT_NXT);
	bool use_compression = m_args.compress &  (m_args.sprites ? COMPRESS_SPRITES : COMPRESS_TILES);

	if (data_size == 0)
		return;

	if (m_args.tile_planar4)
	{
		// Convert 4-bit chunky to planar
		c2p(m_tiles, data_size);
	}

	if (m_args.bank_size > BANKSIZE_NONE)
	{
		m_bank_count = 0;

		while (data_size > 0)
		{
			uint32_t bank_size = (data_size < m_bank_size ? data_size : m_bank_size);
			
			if (bank_size == 0)
				break;
			
			create_series_filename(out_filename, m_args.out_filename, extension, use_compression, m_bank_count);
			
			if (m_args.asm_mode > ASMMODE_NONE)
			{
				strcpy(m_asm_labels[m_bank_count], out_filename);
				
				alphanumeric_to_underscore(m_asm_labels[m_bank_count]);
			}
			
			FILE *p_file = fopen(out_filename, "wb");

			if (p_file == NULL)
			{
				exit_with_msg("Can't create file %s.\n", out_filename);
			}
			
			write_next_bitmap_file(p_file, out_filename, &m_tiles[m_bank_count * m_bank_size], bank_size, use_compression);
			
			fclose(p_file);

			if (m_args.preview)
			{
				uint32_t tile_count = bank_size / tile_size;
				uint32_t bitmap_width = 0, bitmap_height = 0;
				
				create_series_filename(out_filename, m_args.out_filename, "_preview.png", false, m_bank_count);
				
				//write_png(out_filename, &m_tiles[m_bank_count * m_bank_size], m_image_width, bank_size / m_image_width);
				write_tiles_png(out_filename, m_tile_width, m_tile_height, tile_offset, tile_count, m_args.tiled_width, &bitmap_width, &bitmap_height);
				
				tile_offset += tile_count;
			}
			
			m_bank_count++;
			data_size -= bank_size;
		}
	}
	else
	{
		create_filename(out_filename, m_args.out_filename, extension, use_compression);
		
		FILE *p_file = fopen(out_filename, "wb");
		
		if (p_file == NULL)
		{
			exit_with_msg("Can't create file %s.\n", out_filename);
		}

		write_next_bitmap_file(p_file, out_filename, m_tiles, data_size, use_compression);
		
		if (m_args.preview)
		{
			uint32_t bitmap_width = 0, bitmap_height = 0;

			create_filename(out_filename, m_args.out_filename, "_tileset_preview.png", false);

			write_tiles_png(out_filename, m_tile_width, m_tile_height, 0, m_tile_count, m_args.tiled_width, &bitmap_width, &bitmap_height);
		}
		
		fclose(p_file);
	}
	
	if (m_args.asm_mode > ASMMODE_NONE)
	{
		if (m_args.asm_sequence && m_bank_count > 0)
		{
			write_asm_sequence();
			
			if (m_args.asm_mode == ASMMODE_Z80ASM)
			{
				write_header_sequence();
			}
		}
	}
}

static void write_blocks()
{
	char block_filename[256] = { 0 };
	create_filename(block_filename, m_args.out_filename, EXT_NXB, m_args.compress & COMPRESS_BLOCKS);
	FILE *p_file = fopen(block_filename, "wb");
	
	if (p_file == NULL)
	{
		exit_with_msg("Can't create file %s.\n", block_filename);
	}
	
	uint32_t block_bytes = m_args.block_16bit ? 2 : 1;
	uint32_t block_size = block_bytes * m_block_count * m_block_width * m_block_height;
	uint8_t *p_buffer = malloc(block_size);
	uint8_t *p_offset = p_buffer;
	
	for (int i = 0; i < m_block_count; i++)
	{
		for (int y = 0; y < m_block_height; y++)
		{
			for (int x = 0; x < m_block_width; x++)
			{
				uint32_t block_index = i * m_block_size + m_block_width * y + x;
				
				memcpy(p_offset, &m_blocks[block_index], block_bytes);
				
				p_offset += block_bytes;
			}
		}
	}
	
	write_file(p_file, block_filename, p_buffer, block_size, false, m_args.compress & COMPRESS_BLOCKS);
	
	free(p_buffer);
	fclose(p_file);
}

static void write_tiled_files(uint32_t image_width, uint32_t image_height, uint32_t tile_width, uint32_t tile_height, uint32_t block_width, uint32_t block_height, bool use_tsx)
{
	char name[256] = { 0 }, png_filename[256] = { 0 }, tmx_filename[256] = { 0 }, tsx_filename[256] = { 0 };
	create_name(name, m_args.out_filename);
	create_filename(png_filename, m_args.out_filename, "_tileset.png", false);
	create_filename(tmx_filename, m_args.out_filename, EXT_TMX, false);
	create_filename(tsx_filename, m_args.out_filename, EXT_TSX, false);
	FILE *p_tmx_file = fopen(tmx_filename, "w");
	
	uint32_t tile_count = MIN(m_tile_count, m_args.map_16bit ? 512 : 256);
	uint32_t bitmap_width = 0, bitmap_height = 0;
	
	write_tiles_png(png_filename, tile_width, tile_height, 0, tile_count, m_args.tiled_width, &bitmap_width, &bitmap_height);
	
	uint32_t map_width = image_width / (tile_width * block_width);
	uint32_t map_height = image_height / (tile_height * block_height);
	uint16_t map_mask = m_args.map_16bit ? 0x1ff : 0xff;
	uint32_t first_gid = 1;
	
	fprintf(p_tmx_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(p_tmx_file, "<map version=\"1.5\" tiledversion=\"1.7.0\" orientation=\"orthogonal\" renderorder=\"right-down\" width=\"%d\" height=\"%d\" tilewidth=\"%d\" tileheight=\"%d\" infinite=\"0\" nextlayerid=\"2\" nextobjectid=\"1\">\n", map_width, map_height, tile_width, tile_height);
	
	if (use_tsx)
	{
		fprintf(p_tmx_file, " <tileset firstgid=\"%d\" source=\"%s\"/>\n", first_gid, tsx_filename);
	}
	else
	{
		fprintf(p_tmx_file, "<tileset firstgid=\"%d\" name=\"%s\" tilewidth=\"%d\" tileheight=\"%d\" tilecount=\"%d\" columns=\"%d\">\n", first_gid, name, tile_width, tile_height, tile_count, bitmap_width / tile_width);
		fprintf(p_tmx_file, " <image source=\"%s\" width=\"%d\" height=\"%d\"/>\n", png_filename, bitmap_width, bitmap_height);
		fprintf(p_tmx_file, "</tileset>\n");
	}
	fprintf(p_tmx_file, " <layer id=\"1\" name=\"Tile Layer 1\" width=\"%d\" height=\"%d\">\n", map_width, map_height);
	fprintf(p_tmx_file, "  <data encoding=\"csv\">\n");

	if (m_args.map_y)
	{
		for (int x = 0; x < map_width; x++)
		{
			for (int y = 0; y < map_height; y++)
			{
				uint16_t tile_id = m_map[y * map_width + x];
				uint8_t tile_flags = attributes_to_tiled_flags(tile_id >> 8);
				uint32_t tile_value = ((first_gid + tile_id) & map_mask) | (tile_flags << 28);
				
				if (x == map_width-1 && y == map_height-1)
					fprintf(p_tmx_file, "%u", tile_value);
				else
					fprintf(p_tmx_file, "%u,", tile_value);
			}
			fprintf(p_tmx_file, "\n");
		}
	}
	else
	{
		for (int y = 0; y < map_height; y++)
		{
			for (int x = 0; x < map_width; x++)
			{
				uint16_t tile_id = m_map[y * map_width + x];
				uint8_t tile_flags = attributes_to_tiled_flags(tile_id >> 8);
				uint32_t tile_value = ((first_gid + tile_id) & map_mask) | (tile_flags << 28);
				
				if (x == map_width-1 && y == map_height-1)
					fprintf(p_tmx_file, "%u", tile_value);
				else
					fprintf(p_tmx_file, "%u,", tile_value);
			}
			fprintf(p_tmx_file, "\n");
		}
	}
	
	fprintf(p_tmx_file, "  </data>\n");
	fprintf(p_tmx_file, " </layer>\n");
	fprintf(p_tmx_file, "</map>\n");

	fclose(p_tmx_file);
	
	if (use_tsx)
	{
		FILE *p_tsx_file = fopen(tsx_filename, "w");
		
		fprintf(p_tsx_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(p_tsx_file, "<tileset version=\"1.4\" tiledversion=\"1.4.1\" name=\"%s\" tilewidth=\"%d\" tileheight=\"%d\" tilecount=\"%d\" columns=\"%d\">\n", name, tile_width, tile_height, tile_count, bitmap_width / tile_width);
		fprintf(p_tsx_file, " <image source=\"%s\" width=\"%d\" height=\"%d\"/>\n", png_filename, bitmap_width, bitmap_height);
		fprintf(p_tsx_file, "</tileset>\n");

		fclose(p_tsx_file);
	}
}

static void write_map(uint32_t image_width, uint32_t image_height, uint32_t tile_width, uint32_t tile_height, uint32_t block_width, uint32_t block_height)
{
	char map_filename[256] = { 0 };
	create_filename(map_filename, m_args.out_filename, EXT_NXM, m_args.compress & COMPRESS_MAP);
	FILE *p_file = fopen(map_filename, "wb");
	
	if (p_file == NULL)
	{
		exit_with_msg("Can't create file %s.\n", map_filename);
	}
	
	uint32_t map_width = image_width / (tile_width * block_width);
	uint32_t map_height = image_height / (tile_height * block_height);
	uint32_t map_bytes = m_args.map_16bit ? 2 : 1;
	uint32_t map_size = map_bytes * map_width * map_height;
	uint8_t *p_buffer = malloc(map_size);
	uint8_t *p_offset = p_buffer;
		
	printf("Map Size = %d x %d\n", map_width, map_height);
	
	if (m_args.map_y)
	{
		for (int x = 0; x < map_width; x++)
		{
			for (int y = 0; y < map_height; y++)
			{
				memcpy(p_offset, &m_map[y * map_width + x], map_bytes);
				
				p_offset += map_bytes;
			}
		}
	}
	else
	{
		for (int y = 0; y < map_height; y++)
		{
			for (int x = 0; x < map_width; x++)
			{
				memcpy(p_offset, &m_map[y * map_width + x], map_bytes);
				
				p_offset += map_bytes;
			}
		}
	}
	
	write_file(p_file, map_filename, p_buffer, map_size, m_args.map_16bit, m_args.compress & COMPRESS_MAP);
	
	free(p_buffer);
	fclose(p_file);
	
	if (m_args.tiled_output)
	{
		write_tiled_files(image_width, image_height, tile_width, tile_height, block_width, block_height, m_args.tiled_tsx);
	}
}

static match_t check_tile(int i)
{
	match_t match = MATCH_XY;
	
	for (int j = 0; j < m_tile_size; j++)
	{
		int ti = i * m_tile_size + j;
		int index = m_tile_count * m_tile_size + j;
		
		if (m_args.colors_4bit)
		{
			ti >>= 1;
			index >>= 1;
		}
		
		if (m_tiles[ti] != m_tiles[index])
		{
			match &= ~MATCH_XY;
			break;
		}
	}
	
	return match;
}

static match_t check_tile_rotate(int i)
{
	match_t match = MATCH_XY | MATCH_MIRROR_Y | MATCH_MIRROR_X | MATCH_MIRROR_XY;
	match_t match_rot = MATCH_XY | MATCH_ROTATE | MATCH_MIRROR_Y | MATCH_MIRROR_X | MATCH_MIRROR_XY;
	int tile_offset = i * m_tile_size;
	
	for (int y = 0; y < m_tile_height; y++)
	{
		for (int x = 0; x < m_tile_width; x++)
		{
			int x_r = m_tile_width - x - 1;
			int y_r = m_tile_height - y - 1;
			int offset = y * m_tile_width + x;
			int offset_x_r = y * m_tile_width + x_r;
			int offset_y_r = y_r * m_tile_width + x;
			int offset_xy_r = y_r * m_tile_width + x_r;
			int offset_rot = x * m_tile_height + y_r;
			int ti = tile_offset + offset;
			int ti_x_r = tile_offset + offset_x_r;
			int ti_y_r = tile_offset + offset_y_r;
			int ti_xy_r = tile_offset + offset_xy_r;
			int index = m_tile_count * m_tile_size + offset;
			int index_rot = m_tile_count * m_tile_size + offset_rot;
			
			uint8_t px, px_x_r, px_y_r, px_xy_r, px_other, px_other_rot;
			
			if (m_args.colors_4bit)
			{
				px = (x & 1 ? m_tiles[ti >> 1] & 0xf : m_tiles[ti >> 1] >> 4);
				px_x_r = (x_r & 1 ? m_tiles[ti_x_r >> 1] & 0xf : m_tiles[ti_x_r >> 1] >> 4);
				px_y_r = (x & 1 ? m_tiles[ti_y_r >> 1] & 0xf : m_tiles[ti_y_r >> 1] >> 4);
				px_xy_r = (x_r & 1 ? m_tiles[ti_xy_r >> 1] & 0xf : m_tiles[ti_xy_r >> 1] >> 4);
				px_other = (x & 1 ? m_tiles[index >> 1] & 0xf : m_tiles[index >> 1] >> 4);
				px_other_rot = (y_r & 1 ? m_tiles[index_rot >> 1]  & 0xf : m_tiles[index_rot >> 1] >> 4);
			}
			else
			{
				px = m_tiles[ti];
				px_x_r = m_tiles[ti_x_r];
				px_y_r = m_tiles[ti_y_r];
				px_xy_r = m_tiles[ti_xy_r];
				px_other = m_tiles[index];
				px_other_rot = m_tiles[index_rot];
			}
			
			if (px != px_other)
			{
				match &= ~MATCH_XY;
			}
			
			if (px_y_r != px_other)
			{
				match &= ~MATCH_MIRROR_Y;
			}
			
			if (px_x_r != px_other)
			{
				match &= ~MATCH_MIRROR_X;
			}
			
			if (px_xy_r != px_other)
			{
				match &= ~MATCH_MIRROR_XY;
			}
			
			if (px != px_other_rot)
			{
				match_rot &= ~MATCH_XY;
			}
			
			if (px_y_r != px_other_rot)
			{
				match_rot &= ~MATCH_MIRROR_X;
			}
			
			if (px_x_r != px_other_rot)
			{
				match_rot &= ~MATCH_MIRROR_Y;
			}
			
			if (px_xy_r != px_other_rot)
			{
				match_rot &= ~MATCH_MIRROR_XY;
			}
		}
	}
	
	if (!(match & MATCH_ANY))
	{
		if ((match_rot & MATCH_ANY) && (!m_args.tile_nomirror))
		{
			match = match_rot;
		}
	}
	
	if (match & MATCH_MIRROR_XY)
	{
		match &= ~MATCH_MIRROR_XY;
		match |= MATCH_MIRROR_X | MATCH_MIRROR_Y;
	}
	
	return match;
}

static int get_tile(int tx, int ty, uint8_t *attributes)
{
	if (m_args.debug)
	{
		printf("Tile Size = %d x %d\n", m_tile_width, m_tile_height);
		printf("Image Size = %d x %d\n", m_image_width, m_image_height);
		printf("Tile x = %04x, y = %04x\n", tx, ty);
	}

	if (m_args.tile_ldws && !m_args.tile_y)
	{
		for (int x = 0; x < m_tile_width; x++)
		{
			if (m_args.debug)
				printf("\n%04x: ", x);
			
			for (int y = 0; y < m_tile_height; y++)
			{
				int tile_size = m_tile_size * m_tile_count;
				int ti = tile_size + y * m_tile_width + x;
				int px = tx + x, py = ty + y;
				int index = py * m_image_width + px;
				
				if (px < 0 || px >= m_image_width || py < 0 || py >= m_image_height)
					continue;
				
				uint8_t pix = m_next_image[index];
				
				if (m_args.debug)
				{
					printf("%02x ", pix);
				}
				
				if (m_args.colors_1bit)
				{
					m_tiles[ti >> 3] <<= 1;
					m_tiles[ti >> 3] |= pix ? 1 : 0;
				}
				else
				if (m_args.colors_4bit)
				{
					if (ti & 1)
					{
						m_tiles[ti >> 1] |= (pix & 0xf);
					}
					else
					{
						m_tiles[ti >> 1] = (pix << 4) & 0xf0;
					}
					
					if (m_chunk_size >> 4)
					{
						*attributes = (pix & 0xf0);
					}
				}
				else
				{
					m_tiles[ti] = pix;
				}
			}
		}
	}
	else
	{
		for (int y = 0; y < m_tile_height; y++)
		{
			if (m_args.debug)
				printf("\n%04x: ", y);
			for (int x = 0; x < m_tile_width; x++)
			{
				int tile_size = m_tile_size * m_tile_count;
				int ti = tile_size + y * m_tile_width + x;
				int px = tx + x, py = ty + y;
				int index = py * m_image_width + px;
				
				if (px < 0 || px >= m_image_width || py < 0 || py >= m_image_height)
					continue;
				
				uint8_t pix = m_next_image[index];
				
				if (m_args.debug)
				{
					printf("%02x ", pix);
				}
				
				if (m_args.colors_1bit)
				{
					m_tiles[ti >> 3] <<= 1;
					m_tiles[ti >> 3] |= pix ? 1 : 0;
				}
				else
				if (m_args.colors_4bit)
				{
					if (ti & 1)
					{
						m_tiles[ti >> 1] |= (pix & 0xf);
					}
					else
					{
						m_tiles[ti >> 1] = (pix << 4) & 0xf0;
					}

					if (m_chunk_size >> 4)
					{
						*attributes = (pix & 0xf0);
					}
				}
				else
				{
					m_tiles[ti] = pix;
				}
			}
		}
	}
	
	if (m_args.debug)
		printf("\n");

	uint32_t tile_index = m_tile_count;
	match_t match = MATCH_NONE;
	
	if (m_args.tile_norepeat || m_args.tile_norotate || m_args.tile_nomirror)
	{
		for (int i = 0; i < m_tile_count; i++)
		{
			match = ((m_args.tile_norotate || m_args.tile_nomirror) ? check_tile_rotate(i) : check_tile(i));
			
			if (match != MATCH_NONE)
			{
				m_chunk_size = m_tile_size;
				tile_index = i;
				
				if (m_args.tile_norotate || m_args.tile_nomirror)
				{
					if (m_args.map_sms)
					{
						// H-flip differs from the next
						*attributes |= (match >> 2) & 0x02;
						// V-flip bit is the same as the Next
						*attributes |= (match & 0x04);
						// Note: there is no rotate on the SMS
					}
					else
					{
						*attributes |= (match & 0xe);
					}
				}
				break;
			}
		}
	}
	
	if (match == MATCH_NONE)
	{
		m_tile_count++;
	}
	
	*attributes |= m_args.tile_pal << 4;
	
	if (m_args.debug)
		printf("Tile Index = %04x, Tile Count = %d\n", tile_index, m_tile_count);
	
	return tile_index;
}

static int get_block(int tbx, int tby)
{
	if (m_args.debug)
		printf("\nBlock = %04x,%04x\n", tbx, tby);
	
	if (m_block_width == 1 && m_block_height == 1)
	{
		uint8_t attributes = 0;
		return get_tile(tbx, tby, &attributes);
	}
	
	for (int y = 0; y < m_block_height; y++)
	{
		for (int x = 0; x < m_block_width; x++)
		{
			uint8_t attributes = 0;
			m_blocks[(m_block_count * m_block_width * m_block_height) + (y * m_block_width) + x] = get_tile(tbx + (x * m_tile_width), tby + (y * m_tile_height), &attributes);
		}
	}
	
	uint32_t block_index = m_block_count;
	bool found = false;
	
	if (m_args.block_norepeat)
	{
		for (int i = 0; i < m_block_count; i++)
		{
			uint32_t block_size = m_block_width * m_block_height;
			
			found = true;
			
			for (int j = 0; j < block_size; j++)
			{
				if (m_blocks[i * block_size + j] != m_blocks[m_block_count * block_size + j])
				{
					found = false;
					break;
				}
			}
			
			if (found)
			{
				m_chunk_size = block_size;
				block_index = i;
				break;
			}
		}
	}
	
	if (!found)
	{
		if (m_args.debug)
		{
			printf("New Block %d =\n", m_block_count);
			
			for (int i = 0; i < m_block_height; i++)
			{
				for (int j = 0; j < m_block_width; j++)
				{
					printf("%02x ", m_blocks[m_block_count * (m_block_height * m_block_width) + (i * m_block_width) + j]);
				}
				
				printf("\n");
			}
		}
		
		m_block_count++;
	}
	
	return block_index;
}

static void process_tiles()
{
	if (m_args.bitmap)
	{
		m_tile_width = m_image_width;
		m_tile_height = m_image_height;
		m_tile_size = m_tile_width * m_tile_height;
		m_tile_count = 1;
		m_args.map_none = true;
		
		if (m_args.tile_y)
		{
			for (int x = 0; x < m_tile_width; x++)
			{
				for (int y = 0; y < m_tile_height; y++)
				{
					m_tiles[x * m_tile_height + y] = m_next_image[y * m_tile_width + x];
				}
			}
		}
		else
		{
			for (int i = 0; i < m_tile_size; i++)
			{
				if (m_args.colors_4bit)
				{
					if (i & 1)
					{
						m_tiles[i >> 1] |= m_next_image[i] & 0xf;
					}
					else
					{
						m_tiles[i >> 1] = m_next_image[i] << 4;
					}
				}
				else
				{
					m_tiles[i] = m_next_image[i];
				}
			}
		}
	}
	else
	{
		uint32_t map_width = m_image_width / (m_tile_width * m_block_width);
		uint32_t map_height = m_image_height / (m_tile_height * m_block_height);
	
		if (m_args.tile_y)
		{
			for (int x = 0; x < map_width; x++)
			{
				for (int y = 0; y < map_height; y++)
				{
					if (m_block_width == 1 && m_block_height == 1)
					{
						uint8_t attributes = 0;
						uint32_t ti = m_args.tile_offset + get_tile(x * m_tile_width, y * m_tile_height, &attributes);
						uint16_t map_mask = (m_args.map_16bit ? 0x1ff : 0xff);
						
						m_map[x * (m_image_height / m_tile_height) + y] = (ti & map_mask) | (attributes << 8);
					}
					else
					{
						uint32_t ti = get_block(x * m_tile_width * m_block_width, y * m_tile_height * m_block_height);
						m_map[x + map_height * x] = ti;
					}
				}
			}
		}
		else
		{
			for (int y = 0; y < map_height; y++)
			{
				for (int x = 0; x < map_width; x++)
				{
					if (m_block_width == 1 && m_block_height == 1)
					{
						uint8_t attributes = 0;
						uint32_t ti = m_args.tile_offset + get_tile(x * m_tile_width, y * m_tile_height, &attributes);
						uint16_t map_mask = (m_args.map_16bit ? 0x1ff : 0xff);
						
						m_map[y * (m_image_width / m_tile_width) + x] = (ti & map_mask) | (attributes << 8);
					}
					else
					{
						uint32_t ti = get_block(x * m_tile_width * m_block_width, y * m_tile_height * m_block_height);
						m_map[y * map_width + x] = ti;
					}
				}
			}
		}
	}
	
	if (m_args.map_16bit)
	{
		if (m_tile_count > 512)
			printf("Warning tile count > 512!\n");
	}
	else
	{
		if (m_tile_count > 256)
			printf("Warning tile count > 256!\n");
	}
}

static uint8_t attributes_to_tiled_flags(uint8_t attributes)
{
	uint8_t tile_flags = attributes;
	
	if (attributes & MATCH_ROTATE)
	{
		if ((attributes & (MATCH_MIRROR_X | MATCH_MIRROR_Y)) == (MATCH_MIRROR_X | MATCH_MIRROR_Y))
			tile_flags = TILED_DIAG | TILED_VERT;
		else if (attributes & MATCH_MIRROR_X)
			tile_flags = TILED_DIAG;
		else  if (attributes & MATCH_MIRROR_Y)
			tile_flags = TILED_DIAG | TILED_HORIZ | TILED_VERT;
		else
			tile_flags = TILED_DIAG | TILED_HORIZ;
	}
	
	return tile_flags;
}

static uint8_t tiled_flags_to_attributes(uint8_t tile_flags)
{
	uint8_t attributes = tile_flags;
		
	if (tile_flags & TILED_DIAG)
	{
		if ((tile_flags & TILED_HORIZ_VERT) == TILED_HORIZ_VERT)
			attributes = MATCH_ROTATE | MATCH_MIRROR_Y;
		else if (tile_flags & TILED_HORIZ)
			attributes = MATCH_ROTATE;
		else  if (tile_flags & TILED_VERT)
			attributes = MATCH_ROTATE | MATCH_MIRROR_X | MATCH_MIRROR_Y;
		else
			attributes = MATCH_ROTATE | MATCH_MIRROR_X;
	}
	
	return attributes;
}

static void parse_tile(char *line, int first_gid, int *tile_count)
{
	char *pch = strtok(line, ",\r\n");
	
	while (pch != NULL)
	{
		uint32_t tile_id = atoi(pch) - first_gid;
		
		if (tile_id == -1)
			tile_id = m_args.tiled_blank;
		
		uint8_t attributes = tiled_flags_to_attributes(tile_id >> 28);

		m_map[(*tile_count)++] = (tile_id & TILED_TILEID_MASK) | (attributes << 8);
		
		pch = strtok(NULL, ",\r\n");
	}
}

/* static void parse_csv(char *filename, int *tile_count)
{
	char line[512];
	FILE *csv_file = fopen(filename, "r");
	
	while (fgets(line, 512, csv_file))
	{
		parse_tile(line, 0, tile_count);
	}
} */

static bool get_str(char *line, char *name, char *string)
{
	char *pch = NULL, *pch_s = NULL, *pch_e = NULL;
	
	if ((pch = strstr(line, name)))
	{
		pch_s = pch + strlen(name) + 1;
		if ((pch_e = strstr(pch_s, "\"")))
		{
			uint32_t len = pch_e - pch_s;
			strncpy(string, pch_s, len);
			string[len] = '\0';
			
			return true;
		}
	}
	
	return false;
}

static bool get_int(char *line, char *name, int *value)
{
	char string[32];
	
	memset(string, 0, 32);
	
	if (!get_str(line, name, string))
		return false;
	
	*value = atoi(string);
		
	return true;
}

static void parse_tsx(char *filename, char *bitmap_filename)
{
	char line[512];
	FILE *tsx_file = fopen(filename, "r");
	
	if (tsx_file == NULL)
	{
		exit_with_msg("Can't open tsx file %s.\n", filename);
	}
	
	while (fgets(line, 512, tsx_file))
	{
		if (strstr(line, "<image"))
		{
			get_str(line, "source=", bitmap_filename);
		}
	}
	
	fclose(tsx_file);
}

static void parse_tmx(char *filename, char *bitmap_filename)
{
	char line[512], string[32];
	char tileset_filename[256] = { 0 };
	FILE *tmx_file = fopen(filename, "r");
	
	if (tmx_file == NULL)
	{
		exit_with_msg("Can't open tmx file %s.\n", filename);
	}
	
	int map_width = 0, map_height = 0;
	int tile_width = 0, tile_height = 0;
	int tile_count = 0;
	int first_gid = 0;
	bool is_data = false;
	
	while (fgets(line, 512, tmx_file))
	{
		if (strstr(line, "<map"))
		{
			get_int(line, "width=", &map_width);
			get_int(line, "height=", &map_height);
			get_int(line, "tilewidth=", (int *)&tile_width);
			get_int(line, "tileheight=", (int *)&tile_height);
			
			continue;
		}
		
		if (strstr(line, "<tileset"))
		{
			get_int(line, "firstgid=", &first_gid);
			get_str(line, "source=", tileset_filename);
			
			if (!m_args.tile_none && strlen(tileset_filename) != 0)
			{
				parse_tsx(tileset_filename, bitmap_filename);
			}
			
			continue;
		}
		
		if (strstr(line, "<image"))
		{
			get_str(line, "source=", bitmap_filename);
		}
		
		if (strstr(line, "<data"))
		{
			if (get_str(line, "encoding=", string))
			{
				if (strstr(line, "csv"))
				{
					is_data = true;
				}
			}
			
			continue;
		}
		
		if (strstr(line, "</data>"))
		{
			is_data = false;
			continue;
		}
		
		if (!is_data)
			continue;
		
		parse_tile(line, first_gid, &tile_count);
	}
	
	fclose(tmx_file);
	
	int image_width = map_width * tile_width;
	int image_height = map_height * tile_height;

	if (!m_args.map_none)
	{
		write_map(image_width, image_height, tile_width, tile_height, 1, 1);
	}
}

int process_file()
{
	printf("Processing '%s'...\n", m_args.out_filename != NULL ? m_args.out_filename : m_args.in_filename);
	
	// Create file names for raw image file and, if separate, raw palette file.
	create_filename(m_bitmap_filename, m_args.out_filename != NULL ? m_args.out_filename : m_args.in_filename, EXT_NXI, m_args.compress & COMPRESS_BITMAP);
	
	if (!strcmp(m_args.in_filename, m_bitmap_filename))
	{
		exit_with_msg("Input file and output file cannot have the same name (%s == %s).\n", m_args.in_filename, m_bitmap_filename);
	}
	
	const char *p_ext = strrchr((const char *)m_args.in_filename,'.');
	
	if (p_ext != NULL)
	{
		if (strcasecmp(p_ext, EXT_TMX) == 0)
		{
			m_args.tiled = true;
			
			parse_tmx(m_args.in_filename, m_bitmap_filename);
			
			if (m_args.out_filename == NULL || m_args.in_filename == m_args.out_filename)
			{
				m_args.out_filename = m_bitmap_filename;
			}
			
			m_args.in_filename = m_bitmap_filename;
			
			if (!m_args.tile_none && strlen(m_args.in_filename) != 0)
			{
				printf("Processing '%s'...\n", m_args.in_filename);
			}
			
			p_ext = strrchr(m_args.in_filename,'.');
		}
		
		if (strcasecmp(p_ext, ".png") == 0)
		{
			read_png();
		}
		else if (strcasecmp(p_ext, ".bmp") == 0)
		{
			read_bitmap();
		}
	}
	
	if (m_args.asm_mode > ASMMODE_NONE)
	{
		char asm_filename[256] = { 0 };
		char *open_args = (m_args.asm_file != NULL && !m_args.asm_start ? "a" : "w");
		char *asm_file = (m_args.asm_file != NULL ? m_args.asm_file : m_args.out_filename);
		
		create_filename(asm_filename,asm_file , ".asm", false);
		
		m_asm_file = fopen(asm_filename, open_args);
		
		if (m_asm_file == NULL)
		{
			exit_with_msg("Can't create asm file %s.\n", asm_filename);
		}
		
		if (m_args.asm_file == NULL || m_args.asm_start)
		{
			write_asm_header();
		}
		
		if (m_args.asm_mode == ASMMODE_Z80ASM)
		{
			char header_filename[256] = { 0 };
			
			create_filename(header_filename, asm_file, ".h", false);
			
			m_header_file = fopen(header_filename, open_args);
			
			if (m_header_file == NULL)
			{
				exit_with_msg("Can't create header file '%s' (%s).\n", header_filename, open_args);
			}

			if (m_args.asm_file == NULL || m_args.asm_start)
			{
				write_header_header(asm_file);
			}
		}
	}
	
	if (m_args.bank_size > BANKSIZE_NONE)
	{
		if (m_args.pal_mode == PALMODE_EMBEDDED)
			m_args.pal_mode = PALMODE_EXTERNAL;
	}
	else if (m_args.bitmap)
	{
		// Open the raw image output file.
		m_bitmap_file = fopen(m_bitmap_filename, "wb");
		
		if (m_bitmap_file == NULL)
		{
			exit_with_msg("Can't create raw image file %s.\n", m_bitmap_filename);
		}
	}
	
	if (!m_args.screen)
	{
		process_palette();
	}
	
	if (m_args.pal_file != NULL)
	{
		read_file(m_args.pal_file, (uint8_t *)m_next_palette, sizeof(m_next_palette));
	}
	else
	{
		if (m_args.pal_bgr222)
		{
			create_sms_palette(m_args.color_mode);
		}
		else
		{
			create_next_palette(m_args.color_mode);
		}
	}
	
	read_next_image();
	
	if (m_args.tiles_file != NULL)
	{
		read_file(m_args.tiles_file, (uint8_t *)m_tiles, sizeof(m_tiles));
	}
	
	if (m_args.font)
	{
		write_font();
		
		if (m_args.asm_mode == ASMMODE_Z80ASM)
		{
			if (m_args.asm_file == NULL || m_args.asm_end)
			{
				write_header_footer();
			}
		}
		
		return 1;
	}
	
	if (m_args.screen)
	{
		write_screen();
		
		if (m_args.asm_mode == ASMMODE_Z80ASM)
		{
			if (m_args.asm_file == NULL || m_args.asm_end)
			{
				write_header_footer();
			}
		}
		
		return 1;
	}
	
	if (!m_args.tile_none)
	{
		process_tiles();
	}
	
	write_next_palette();
	
	if (m_args.tiled_file != NULL)
	{
		char bitmap_filename[256] = { 0 };
		
		parse_tmx(m_args.tiled_file, bitmap_filename);
	}
	else if (!m_args.map_none && !m_args.tiled)
	{
		write_map(m_image_width, m_image_height, m_tile_width, m_tile_height, m_block_width, m_block_height);
	}
	
	if (m_args.bitmap)
	{
		write_next_bitmap();
	}
	else
	{
		if (m_args.sprites)
		{
			printf("Sprite Count = %d\n", m_tile_count);
			
			write_tiles_sprites();
		}
		else if (!m_args.tile_none)
		{
			if (m_block_count > 0)
			{
				printf("Block Count = %d\n", m_block_count);
				
				write_blocks();
			}
			
			printf("Tile Offset = %d\n", m_args.tile_offset);
			printf("Tile Palette = %d\n", m_args.tile_pal);
			printf("Tile Count = %d\n", m_tile_count);
			
			write_tiles_sprites();
		}
	}
	
	if (m_args.asm_mode == ASMMODE_Z80ASM)
	{
		if (m_args.asm_file == NULL || m_args.asm_end)
		{
			write_header_footer();
		}
	}
	
	return 1;
}

int main(int argc, char *argv[])
{
	atexit(exit_handler);

	// Parse program arguments.
	if (!parse_args(argc, argv, &m_args))
	{
		exit(EXIT_FAILURE);
	}
	
	if (strstr(m_args.in_filename, "*"))
	{
		char **filename;
		glob_t gstruct;
		int count = 0;
		int ret;
		
		ret = glob(m_args.in_filename, GLOB_ERR , NULL, &gstruct);
		
		// check for errors
		if(ret != 0)
		{
			if(ret == GLOB_NOMATCH)
				fprintf(stderr,"No matches\n");
			else
				fprintf(stderr,"Some kinda glob error\n");
			
			exit(EXIT_FAILURE);
		}

		// success, output found filenames
		printf("Found %u filename matches\n", (unsigned) gstruct.gl_pathc);
		filename = gstruct.gl_pathv;
		
		if (m_args.asm_start_auto)
		{
			m_args.asm_start = true;
		}
		
		while(*filename)
		{
			m_args.in_filename = *filename;
			m_args.out_filename = *filename;
			
			m_tile_count = 0;
			m_bank_section_index = 0;
			
			process_file();
			close_all();
			
			filename++;
			
			if (m_args.tile_offset_auto)
			{
				m_args.tile_offset += m_tile_count;
			}
			
			if (m_args.tile_pal_auto)
			{
				m_args.tile_pal++;
			}
			
			if (m_args.asm_start_auto)
			{
				m_args.asm_start = false;
			}
			
			if (++count == gstruct.gl_pathc-1)
			{
				if (m_args.asm_end_auto)
				{	
					m_args.asm_end = true;
				}
			}
		}
	}
	else
	{
		process_file();
	}

	for (int i = 0; i < NUM_BANKS; i++)
	{
		if (m_bank_used[i] == 0)
			continue;
		
		printf("BANK_%d = %d bytes used\n", i,  m_bank_used[i]);
	}
	
	return 0;
}
