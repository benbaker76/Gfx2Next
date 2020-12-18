/*******************************************************************************
 * Gfx2Next - ZX Spectrum Next graphics conversion tool
 * 
 * Credits:
 *
 *    Ben Baker - [Gfx2Next](https://www.rustypixels.uk/?page_id=976) Author & Maintainer
 *    Antonio Villena - ZX7b
 *    Einar Saukas - ZX7
 *    Jim Bagley - NextGrab / MapGrabber
 *    Michael Ware - [Tiled2Bin](https://www.rustypixels.uk/?page_id=739)
 *    Stefan Bylun - [NextBmp / NextRaw](https://github.com/stefanbylund/zxnext_bmp_tools)
 *    fyrex^mhm - MegaLZ
 *    lvd^mhm - MegaLZ
 *    Lode Vandevenne -[LodePNG](https://lodev.org/lodepng/)
 *
 * Supports the following ZX Spectrum Next formats:
 * 
 *    .nxb - Block
 *    .nxi - Bitmap
 *    .nxm - Map
 *    .nxp - Palette
 *    .nxt - Tiles
 *    .spr - Sprites
 *    .tmx - [Tiled](https://www.mapeditor.org/)
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include "zx7.h"
#include "megalz.h"
#include "lodepng.h"

#define VERSION						"1.0.1"

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

#define SIZE_8K						8192
#define SIZE_16K					16384
#define SIZE_48K					49152

#define DBL_MAX						1.7976931348623158e+308

#define RGB888(r8,g8,b8)			((r8 << 16) | (g8 << 8) | b8)
#define RGB332(r3,g3,b2)			((r3 << 5) | (g3 << 2) | b2)
#define RGB333(r3,g3,b3)			((r3 << 6) | (g3 << 3) | b3)

#define MIN(x,y)					((x) < (y) ? (x) : (y))
#define MAX(x,y)					((x) > (y) ? (x) : (y))

#define IS_COMPRESSED				(m_args.zx7_mode > ZX7MODE_NONE || m_args.megalz_mode > MEGALZMODE_NONE)

#define EXT_NXM						".nxm"
#define EXT_NXM_COMPRESSED			".nzm"
#define EXT_NXP						".nxp"
#define EXT_NXP_COMPRESSED			".nzp"
#define EXT_NXT						".nxt"
#define EXT_NXT_COMPRESSED			".nzt"
#define EXT_SPR						".spr"
#define EXT_SPR_COMPRESSED			".szr"
#define EXT_NXB						".nxb"
#define EXT_NXB_COMPRESSED			".nzb"
#define EXT_NXI						".nxi"
#define EXT_NXI_COMPRESSED			".nzi"

static void write_easter_egg();

unsigned char m_gf[] = { 0xff,0x81,0x4c,0x00,0xf8,0x00,0x46,0x07,0x04,0x0f,0xc0,0x00,0x00,0x3f,0x5e,0x0f,0x1d,0xf8,0x45,0x01,0x29,0x0e,0xfc,0x1c,0x80,0x14,0x20,0xf0,0xb8,0x13,0xa5,0x41,0xe3,0xa2,0x04,0xc1,0x52,0x0f,0x87,0x22,0x63,0x7f,0x16,0x0f,0x1f,0x04,0xfe,0x92,0x05,0x49,0x09,0x51,0x4a,0x8f,0x12,0x5e,0xa2,0x29,0xc7,0x02,0x9d,0x57,0x06,0xf1,0x02,0x18,0x08,0xf8,0x8c,0x6e,0xa9,0x46,0xe7,0x60,0x38,0x9c,0xa1,0x13,0xcf,0x23,0x07,0x9f,0x90,0x07,0xdf,0xb4,0x27,0xe8,0x0f,0xe7,0xe9,0x57,0xe7,0x0f,0x77,0x4c,0xf3,0xae,0x10,0xba,0x0f,0xfb,0x54,0x06,0xfd,0x2e,0x0f,0xf9,0x8c,0x06,0x9f,0x18,0x26,0x81,0xb9,0x0f,0xf3,0x1c,0x0f,0xf7,0x81,0xc5,0x0f,0xf3,0x9c,0x9c,0x50,0x8f,0x33,0x83,0xd0,0xb3,0xfe,0x9e,0x0f,0x84,0x0c,0xe0,0x00,0x0f,0x58,0x0f,0x1f,0xf0,0x0f,0x46,0x83,0x68,0x0f,0x42,0xc1,0x95,0x0f,0xf1,0x92,0xd8,0xf8,0x2c,0xef,0x12,0xdf,0x9a,0xe3,0xc1,0x6a,0x0f,0x1b,0x14,0xc0,0x0f,0xbf,0x04,0x7f,0x94,0x0f,0x00,0x5f,0xd2,0xcd,0xd1,0xbd,0xbe,0x74,0x74,0x3a,0x30,0xca,0x0f,0x3c,0x03,0x08,0x25,0x7f,0x9f,0x0f,0x6f,0x60,0x0f,0x1c,0x0f,0x42,0x25,0xf1,0xdf,0x2f,0x76,0xe1,0x17,0x0f,0xfd,0x26,0xe0,0xcf,0x5f,0xab,0x4b,0x6e,0x23,0xde,0x6f,0x04,0x0f,0xfb,0xbf,0xa7,0x69,0x0f,0x3f,0xf7,0x19,0x0f,0xc6,0x5f,0x89,0x0f,0xfe,0x5f,0x77,0x93,0x0f,0x3f,0x1a,0xf7,0x67,0x64,0x0f,0x7f,0xa8,0x0f,0xbf,0xd1,0x0f,0xfc,0xdf,0x6f,0x1a,0x0f,0xfe,0x4a,0x4f,0x0f,0xfe,0x99,0x2f,0xfe,0x30,0xaf,0x4c,0xb0,0x9c,0x0f,0xa0,0xcf,0xf3,0x9e,0x0f,0x62,0x77,0xf7,0xfc,0x53,0x92,0x13,0xfd,0xdf,0x3f,0xa5,0x0f,0xfc,0x0f,0x4f,0x7f,0x0f,0x7a,0x51,0x04,0x3a,0x55,0x0f,0xf3,0x59,0x0f,0x29,0xf3,0xe6,0x30,0xbf,0xeb,0x0f,0x9e,0x49,0x13,0xf7,0xcc,0xff,0xac,0x7f,0x9e,0x79,0xf7,0xf9,0x88,0x0f,0x2d,0x7f,0xef,0xbe,0x53,0xf1,0x12,0x0f,0xfd,0x83,0xaa,0xce,0xe3,0x69,0x0f,0x89,0x27,0xef,0x8f,0x93,0x0f,0xf3,0x23,0x0f,0x1f,0x49,0x0f,0xe8,0x3f,0xa6,0x0f,0x87,0x69,0x8b,0xbe,0xbf,0x54,0xef,0xa8,0x07,0xcc,0xab,0x0f,0xe2,0x04,0xde,0xc4,0x0f,0xfd,0xcf,0xac,0xc3,0xde,0x9b,0x3f,0xd7,0x09,0x59,0xe3,0x9e,0x32,0x5f,0xfc,0xe8,0x0f,0xc0,0x09,0xbe,0x0f,0x66,0x0f,0xb6,0x1d,0x12,0x07,0x39,0x0f,0x80,0x00,0x64,0x4a,0x3f,0x03,0xce,0x0f,0x7e,0x0d,0xc8,0x3a,0x7f,0x01,0x97,0x9f,0x5a,0xed,0x06,0x00,0x5e,0x0f,0x18,0xac,0x63,0x82,0x27,0xc3,0x92,0x7f,0xf1,0x52,0x0f,0x80,0x0f,0x35,0xdf,0xf3,0xf4,0x9a,0x68,0x74,0x0f,0x03,0x74,0x0f,0xc9,0x50,0xdf,0xe6,0x0f,0x50,0x27,0xdf,0xfd,0x0f,0x94,0x81,0x04,0xef,0xf0,0x3f,0xd2,0x0f,0xa2,0x20,0xcf,0x6d,0x0f,0x71,0xf6,0x35,0x66,0xda,0x0f,0x30,0xc0,0x2a,0x0f,0x88,0x30,0xfe,0x00,0xbd,0x0f,0x30,0xf8,0x19,0x0f,0x83,0x92,0xe0,0x03,0x99,0x0f,0x00,0x7c,0x00,0x05,0x6a,0x0f,0xfc,0xe2,0x54,0xbe,0x0f,0x2e,0x00,0x61,0x97,0x92,0x0f,0xd1,0x44,0x7f,0x56,0x0f,0x00,0x1f,0x8e,0x0f,0x96,0x70,0xa7,0x49,0x00,0x23,0xf8,0x03,0x51,0x0f,0x01,0xb2,0x1f,0xa2,0x70,0xf8,0x08,0x0f,0x02,0x7f,0xe4,0x0f,0x3f,0x69,0x0f,0x04,0x9f,0x1a,0x0f,0x06,0x90,0xb1,0xd1,0xe8,0x32,0x0f,0x32,0x88,0xe0,0x25,0x0f,0xf9,0x1c,0x3f,0xfc,0x8e,0x0f,0x91,0x8f,0x51,0xf0,0x32,0x2f,0x32,0x81,0x90,0x26,0x1f,0x40,0x9f,0x47,0xa0,0x22,0x0f,0x60,0xad,0x2f,0x00,0x40,0x1a,0x0f,0xfe,0x43,0x80,0x08,0x0f,0x0e,0xa1,0xae,0x80,0x00,0x40 };

typedef enum
{
	ZX7MODE_NONE,
	ZX7MODE_STANDARD,
	ZX7MODE_BACKWARDS
} zx7_mode_t;

typedef enum
{
	MEGALZMODE_NONE,
	MEGALZMODE_OPTIMAL,
	MEGALZMODE_GREEDY
} megalz_mode_t;

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
	BANKSIZE_CUSTOM,
	BANKSIZE_XY
} bank_size_t;

typedef struct
{
	char *in_filename;
	char *out_filename;
	bool debug;
	bool font;
	bool bitmap;
	bool bitmap_y;
	bool sprites;
	char *tiles_file;
	bool tile_repeat;
	bool tile_y;
	bool tile_ldws;
	char *tiled_file;
	int tiled_blank;
	bool block_repeat;
	bool block_16bit;
	bool map_none;
	bool map_16bit;
	bool map_y;
	bank_size_t bank_size;
	color_mode_t color_mode;
	bool colors_4bit;
	char *pal_file;
	pal_mode_t pal_mode;
	bool pal_min;
	bool pal_std;
	zx7_mode_t zx7_mode;
	megalz_mode_t megalz_mode;
	asm_mode_t asm_mode;
	bool preview;
} arguments_t;

static arguments_t m_args  =
{
	.in_filename = NULL,
	.out_filename = NULL,
	.debug = false,
	.font = false,
	.bitmap = false,
	.bitmap_y = false,
	.sprites = false,
	.tiles_file = NULL,
	.tile_repeat = false,
	.tile_y = false,
	.tile_ldws = false,
	.tiled_file = NULL,
	.tiled_blank = 0,
	.block_repeat = false,
	.block_16bit = false,
	.map_none = false,
	.map_16bit = false,
	.map_y = false,
	.bank_size = BANKSIZE_NONE,
	.color_mode = COLORMODE_DISTANCE,
	.colors_4bit = false,
	.pal_file = NULL,
	.pal_mode = PALMODE_EXTERNAL,
	.pal_min = false,
	.pal_std = false,
	.zx7_mode = ZX7MODE_NONE,
	.megalz_mode = MEGALZMODE_NONE,
	.asm_mode = ASMMODE_NONE,
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

static uint32_t m_bank_width = 256;
static uint32_t m_bank_height = 64;
static uint32_t m_bank_size = 0;
static uint32_t m_bank_count = 0;

static char m_bank_sections[MAX_BANK_SECTION_COUNT][256] = { { 0 } };
static uint32_t m_bank_section_index = 0;
static uint32_t m_bank_section_count = 0;

static uint32_t m_tile_width = 8;
static uint32_t m_tile_height = 8;
static uint32_t m_tile_size = 8 * 8;
static uint32_t m_tile_count = 0;

static uint32_t m_block_width = 1;
static uint32_t m_block_height = 1;
static uint32_t m_block_size = 1 * 1;
static uint32_t m_block_count = 0;

static uint32_t m_palette_index = 0;
static uint32_t m_chunk_size = 0;

static char m_bitmap_filename[256] = { 0 };
static char m_asm_labels[MAX_LABEL_COUNT][256] = { { 0 } };

static FILE *m_bitmap_file = NULL;
static FILE *m_asm_file = NULL;
static FILE *m_header_file = NULL;

static void exit_handler(void)
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
			uint32_t rgb888 =  get_nearest_color(RGB888(r8, g8, b8), false);
			
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
			m_std_palette_index[i] = rgb888_to_rgb332(get_nearest_color(RGB888(r8, g8, b8), false), COLORMODE_ROUND);
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

static void create_next_palette(color_mode_t color_mode)
{
	// Create the next palette.
	// The RGB888 colors in the BMP palette are converted to RGB333 colors,
	// which are then split in RGB332 and B1 parts.
	uint32_t palette_count = m_args.colors_4bit ? 16 : 256;
	for (int i = 0; i < palette_count; i++)
	{
		// Palette contains ARGB colors.
		uint8_t r8 = m_palette[i * 4 + 1];
		uint8_t g8 = m_palette[i * 4 + 2];
		uint8_t b8 = m_palette[i * 4 + 3];
		uint16_t rgb333 = rgb888_to_rgb333(RGB888(r8, g8, b8), color_mode);
		uint8_t rgb332 = (uint8_t) (rgb333 >> 1);
		uint8_t b1 = (uint8_t) (rgb333 & 1);

		m_next_palette[i] = (b1 << 8) | rgb332;
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
	char buffer[256];
	sprintf(buffer, "gfx2next v%s\n", VERSION);
	printf(buffer);
	printf("Converts an uncompressed 8-bit BMP or PNG file to the Sinclair ZX Spectrum Next graphics format(s).\n");
	printf("Usage:\n");
	printf("  gfx2next [options] <srcfile> [<dstfile>]\n");
	printf("\n");
	printf("Options:\n");
	printf("  -debug                  Output additional debug information.\n");
	printf("  -font                   Sets output to Next font format (.spr).\n");
	printf("  -bitmap                 Sets output to Next bitmap mode (.nxi).\n");
	printf("  -bitmap-y               Get bitmap in Y order first. (Default is X order first).\n");
	printf("  -sprites                Sets output to Next sprite mode (.spr).\n");
	printf("  -tiles-file=<filename>  Load tiles from file in .nxt format.\n");
	printf("  -tile-size=XxY          Sets tile size to X x Y.\n");
	printf("  -tile-repeat            Remove repeating tiles.\n");
	printf("  -tile-y                 Get tile in Y order first. (Default is X order first).\n");
	printf("  -tile-ldws              Get tile in Y order first for ldws instruction. (Default is X order first).\n");
	printf("  -tiled-file=<filename>  Load map from file in .tmx format.\n");
	printf("  -tiled-blank=X          Set the tile id of the blank tile.\n");
	printf("  -block-size=XxY         Sets blocks size to X x Y for blocks of tiles.\n");
	printf("  -block-size=n           Sets blocks size to n bytes for blocks of tiles.\n");
	printf("  -block-repeat           Remove repeating blocks.\n");
	printf("  -block-16bit            Get blocks as 16 bit index for < 256 blocks.\n");
	printf("  -map-none               Don't save a map file (e.g. if you're just adding to tiles).\n");
	printf("  -map-16bit              Save map as 16 bit output.\n");
	printf("  -map-y                  Save map in Y order first. (Default is X order first).\n");
	printf("  -bank-8k                Splits up output file into multiple 8k files.\n");
	printf("  -bank-16k               Splits up output file into multiple 16k files.\n");
	printf("  -bank-48k               Splits up output file into multiple 48k files.\n");
	printf("  -bank-size=XxY          Splits up output file into multiple X x Y files.\n");
	printf("  -bank-sections=name,... Section names for asm files.\n");
	printf("  -color-distance         Use the shortest distance between color values (default).\n");
	printf("  -color-floor            Round down the color values to the nearest integer.\n");
	printf("  -color-ceil             Round up the color values to the nearest integer.\n");
	printf("  -color-round            Round the color values to the nearest integer.\n");
	printf("  -colors-4bit            Use 4 bits per pixel (16 colors). Default is 8 bits per pixel (256 colors).\n");
	printf("                          Get sprites or tiles as 16 colors, top 4 bits of 16 bit map is palette index.\n");
	printf("  -pal-file=<filename>    Load palette from file in .nxp format.\n");
	printf("  -pal-embed              The raw palette is prepended to the raw image file.\n");
	printf("  -pal-ext                The raw palette is written to an external file (.nxp). This is the default.\n");
	printf("  -pal-min                If specified, minimize the palette by removing any duplicated colors, sort\n");
	printf("                          it in ascending order, and clear any unused palette entries at the end.\n");
	printf("                          This option is ignored if the -pal-std option is given.\n");
	printf("  -pal-std                If specified, convert to the Spectrum Next standard palette colors.\n");
	printf("                          This option is ignored if the -colors-4bit option is given.\n");
	printf("  -pal-none               No raw palette is created.\n");
	printf("  -zx7                    Compress the image data using zx7.\n");
	printf("  -zx7-back               Compress the image data using zx7 in reverse.\n");
	printf("  -megalz                 Compress the image data using MegaLZ optimal.\n");
	printf("  -megalz-greedy          Compress the image data using MegaLZ greedy.\n");
	printf("  -z80asm                 Generate header and asm binary include files (in Z80ASM format).\n");
	printf("  -sjasm                  Generate asm binary incbin file (SjASM format).\n");
	printf("  -preview                Generate png preview file(s).\n");
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
			else if (!strcmp(argv[i], "-bitmap"))
			{
				m_args.bitmap = true;
				m_args.map_none = true;
			}
			else if (!strcmp(argv[i], "-bitmap-y"))
			{
				m_args.bitmap_y = true;
			}
			else if (!strcmp(argv[i], "-sprites"))
			{
				m_args.map_none = true;
				m_tile_width = 16;
				m_tile_height = 16;
				m_tile_size = m_tile_width * m_tile_height;
				m_args.tile_repeat = false;
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
			else if (!strcmp(argv[i], "-tile-repeat"))
			{
				m_args.tile_repeat = true;
			}
			else if (!strcmp(argv[i], "-tile-y"))
			{
				m_args.tile_y = true;
			}
			else if (!strcmp(argv[i], "-tile-ldws"))
			{
				m_args.tile_ldws = true;
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
			else if (!strncmp(argv[i], "-block-size=", 12))
			{
				m_block_width = atoi(strtok(&argv[i][12], "x"));
				m_block_height = atoi(strtok(NULL, "x"));
				m_block_size = m_block_width * m_block_height;
				
				printf("Block Size = %d x %d\n", m_block_width, m_block_height);
			}
			else if (!strcmp(argv[i], "-block-repeat"))
			{
				m_args.block_repeat = true;
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
				if (strstr(&argv[i][11], "x"))
				{
					m_args.bank_size = BANKSIZE_XY;
					m_bank_width = atoi(strtok(&argv[i][11], "x"));
					m_bank_height = atoi(strtok(NULL, "x"));
					m_bank_size = m_bank_width * m_bank_height;
					printf("Bank Size = %d x %d = %d\n", m_bank_width, m_bank_height, m_bank_size);
				}
				else
				{
					m_args.bank_size = BANKSIZE_CUSTOM;
					m_bank_size = atoi(&argv[i][11]);
					printf("Bank Size = %d\n", m_bank_size);
				}
			}
			else if (!strncmp(argv[i], "-bank-sections=", 15))
			{
				char *token = strtok(&argv[i][15], ",");

				while (token != NULL)
				{
					strcpy(m_bank_sections[m_bank_section_count++], token);
					token = strtok(NULL, ",");
				}
				
				for (int i = 0; i < m_bank_section_count; i++)
					printf("Bank Section %d = %s\n", i, m_bank_sections[i]);
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
				m_args.colors_4bit = true;
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
			else if (!strcmp(argv[i], "-pal-none"))
			{
				m_args.pal_mode = PALMODE_NONE;
			}
			else if (!strcmp(argv[i], "-zx7"))
			{
				m_args.zx7_mode = ZX7MODE_STANDARD;
			}
			else if (!strcmp(argv[i], "-zx7-back"))
			{
				m_args.zx7_mode = ZX7MODE_BACKWARDS;
			}
			else if (!strcmp(argv[i], "-megalz"))
			{
				m_args.megalz_mode = MEGALZMODE_OPTIMAL;
			}
			else if (!strcmp(argv[i], "-megalz-greedy"))
			{
				m_args.megalz_mode = MEGALZMODE_GREEDY;
			}
			else if (!strcmp(argv[i], "-z80asm"))
			{
				m_args.asm_mode = ASMMODE_Z80ASM;
			}
			else if (!strcmp(argv[i], "-sjasm"))
			{
				m_args.asm_mode = ASMMODE_SJASM;
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

static void create_filename(char *out_filename, const char *in_filename, const char *extension)
{
	strcpy(out_filename, in_filename);

	char *end = strrchr(out_filename, '.');
	end = (end == NULL) ? out_filename + strlen(out_filename) : end;

	strcpy(end, extension);
}

static void create_series_filename(char *out_filename, const char *in_filename, const char *extension, int index)
{
	strcpy(out_filename, in_filename);
	
	char *end = strrchr(out_filename, '.');
	out_filename[end == NULL ? strlen(out_filename) : (int)(end - out_filename)] = '\0';

	snprintf(out_filename, 255, "%s_%d%s", out_filename, index, extension);
}

static void to_upper(char *filename)
{
	int i = 0;
	while(m_bitmap_filename[i])
	{
		filename[i] = toupper(filename[i]);
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
								uint32_t *image_offset)
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

	if (m_image_width * abs(m_image_height) >= file_size)
	{
		fprintf(stderr, "Invalid image size in BMP file.\n");
		return false;
	}

	uint16_t bpp = *((uint16_t *) (m_bmp_header + 28));
	if (bpp != 8)
	{
		fprintf(stderr, "Not an 8-bit BMP file.\n");
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
	if (!is_valid_bmp_file(&palette_offset, &image_offset))
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
	if (fread(m_image, sizeof(uint8_t), m_image_size, in_file) != m_image_size)
	{
		exit_with_msg("Can't read the BMP image data in file %s.\n", m_args.in_filename);
	}
	
	for (int i = 0; i < NUM_PALETTE_COLORS; i++)
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

static void write_png(const char *in_filename, uint8_t *p_image, int width, int height)
{
	unsigned error;
	unsigned char *image = NULL;
	size_t outsize;
	LodePNGState state;
	
	lodepng_state_init(&state);
	
	uint32_t num_palette_colors = (m_args.bitmap && m_args.colors_4bit ? 16 : 256);
	
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
	state.info_png.color.bitdepth = (m_args.bitmap && m_args.colors_4bit ? 4 : 8);
	state.info_raw.colortype = LCT_PALETTE;
	state.info_raw.bitdepth = (m_args.bitmap && m_args.colors_4bit ? 4 : 8);
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

static void write_1bit(const char *filename, uint8_t *data)
{
	m_args.bitmap = true;
	m_args.colors_4bit = true;
	
	uint8_t buffer_decompress[0x800];
	uint8_t buffer_out[0x2000];
	memset(m_next_palette, 0, sizeof(m_next_palette));
	memset(buffer_out, 0, sizeof(buffer_out));
	
	m_next_palette[1] = 0x01ff;
	
	zx7_decompress(data, buffer_decompress);
	
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
	write_1bit("gf.png", m_gf);
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
			fprintf(m_asm_file, "\nSECTION %s\n", m_bank_sections[m_bank_section_index++]);
		else
			fprintf(m_asm_file, "\nSECTION rodata_user\n");
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
	create_filename(sequence_filename, m_args.out_filename, "_sequence");
	
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

static void write_header_header()
{
	char header_filename[256] = { 0 };
	create_filename(header_filename, m_args.out_filename, "_H");
	
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
	create_filename(header_filename, m_args.out_filename, "_sequence");
	
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

static void write_file(FILE *p_file, char *p_filename, uint8_t *p_buffer, uint32_t buffer_size, bool type_16bit)
{
	if (IS_COMPRESSED)
	{
		size_t compressed_size = 0;
		uint8_t *compressed_buffer = NULL;
		
		if (m_args.zx7_mode > ZX7MODE_NONE)
			compressed_buffer = zx7_compress(p_buffer, buffer_size, m_args.zx7_mode == ZX7MODE_BACKWARDS, &compressed_size);
		else
			compressed_buffer = megalz_compress(p_buffer, buffer_size, m_args.megalz_mode, &compressed_size);

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
	uint32_t next_palette_size = m_args.colors_4bit ? NEXT_4BIT_PALETTE_SIZE : NEXT_PALETTE_SIZE;

	if (m_args.pal_mode == PALMODE_EMBEDDED)
	{
		write_file(m_bitmap_file, m_bitmap_filename, (uint8_t *)m_next_palette, next_palette_size, false);
	}
	else if (m_args.pal_mode == PALMODE_EXTERNAL)
	{
		char palette_filename[256] = { 0 };
		
		create_filename(palette_filename, m_bitmap_filename, IS_COMPRESSED ? EXT_NXP_COMPRESSED : EXT_NXP);
		
		FILE *palette_file = fopen(palette_filename, "wb");
	
		if (palette_file == NULL)
		{
			exit_with_msg("Can't create file %s.\n", palette_filename);
		}
	
		write_file(palette_file, palette_filename, (uint8_t *)m_next_palette, next_palette_size, false);
		
		fclose(palette_file);
	}
}

static void write_next_bitmap_file(FILE *bitmap_file, char *bitmap_filename, uint8_t *next_image, uint32_t next_image_size)
{
	write_file(bitmap_file, bitmap_filename, next_image, next_image_size, false);
	
	if (m_args.preview)
	{
		create_filename(m_bitmap_filename, m_args.out_filename, "_preview.png");

		write_png(m_bitmap_filename, m_next_image, m_image_width, m_image_height);
	}
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

static void write_font()
{
	char font_filename[256] = { 0 };
	create_filename(font_filename, m_args.out_filename, IS_COMPRESSED ? EXT_SPR_COMPRESSED : EXT_SPR);
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
	
	write_file(p_file, font_filename, p_buffer, image_size, false);
	
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
			
			create_series_filename(m_bitmap_filename, m_args.out_filename, IS_COMPRESSED ? EXT_NXI_COMPRESSED : EXT_NXI, m_bank_count);

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
			
			int bank_x;
			uint8_t *p_image = NULL;
			
			if (m_args.bank_size == BANKSIZE_XY)
				p_image = get_bank_width_height(m_next_image, m_bank_count, m_bank_width, m_bank_height, bank_size, &bank_x);
			else
				p_image = get_bank(m_next_image + m_bank_count * m_bank_size, bank_size);
			
			write_next_bitmap_file(bitmap_file, m_bitmap_filename, p_image, bank_size);
			
			fclose(bitmap_file);

			if (m_args.preview)
			{
				create_series_filename(m_bitmap_filename, m_args.out_filename, "_preview.png", m_bank_count);

				write_png(m_bitmap_filename, p_image, m_bank_width, m_bank_height);
				//write_png(m_bitmap_filename, p_image, m_bank_width, bank_size / m_bank_width);
				//write_png(m_bitmap_filename, p_image, bank_x, bank_size / bank_x);
			}

			size -= bank_size;
			m_bank_count++;
		}
	}
	else
	{
		write_next_bitmap_file(m_bitmap_file, m_bitmap_filename, m_next_image, m_next_image_size);
	}
	
	if (m_args.asm_mode > ASMMODE_NONE)
	{
		if (m_bank_count > 0)
		{
			write_asm_sequence();
			
			if (m_args.asm_mode == ASMMODE_Z80ASM)
			{
				write_header_sequence();
			}
		}
	}
}

static void write_tiles_sprites()
{
	char out_filename[256] = { 0 };
	int tile_size = m_tile_size * m_tile_count;
		
	if (m_args.colors_4bit)
		tile_size >>= 1;
	
	if (m_args.bank_size > BANKSIZE_NONE)
	{
		m_bank_count = 0;

		while (tile_size > 0)
		{
			int bank_size = (tile_size < m_bank_size ? tile_size : m_bank_size);
			
			if (m_args.sprites)
			{
				create_series_filename(out_filename, m_args.out_filename, IS_COMPRESSED ? EXT_SPR_COMPRESSED : EXT_SPR, m_bank_count);
			}
			else
			{
				create_series_filename(out_filename, m_args.out_filename, IS_COMPRESSED ? EXT_NXT_COMPRESSED : EXT_NXT, m_bank_count);
			}
			
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
			
			write_next_bitmap_file(p_file, out_filename, &m_tiles[m_bank_count * m_bank_size], bank_size);
			
			fclose(p_file);

			if (m_args.preview)
			{
				create_series_filename(out_filename, m_args.out_filename, "_preview.png", m_bank_count);

				write_png(out_filename, &m_tiles[m_bank_count * m_bank_size], m_bank_width, bank_size / m_bank_width);
			}
			
			m_bank_count++;
			tile_size -= bank_size;
		}
	}
	else
	{
		if (m_args.sprites)
		{
			create_filename(out_filename, m_args.out_filename, IS_COMPRESSED ? EXT_SPR_COMPRESSED : EXT_SPR);
		}
		else
		{
			create_filename(out_filename, m_args.out_filename, IS_COMPRESSED ? EXT_NXT_COMPRESSED : EXT_NXT);
		}
		
		FILE *p_file = fopen(out_filename, "wb");
		
		if (p_file == NULL)
		{
			exit_with_msg("Can't create file %s.\n", out_filename);
		}

		write_next_bitmap_file(p_file, out_filename, m_tiles, tile_size);
		
		fclose(p_file);
	}
	
	if (m_args.asm_mode > ASMMODE_NONE)
	{
		if (m_bank_count > 0)
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
	create_filename(block_filename, m_args.out_filename, IS_COMPRESSED ? EXT_NXB_COMPRESSED : EXT_NXB);
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
	
	write_file(p_file, block_filename, p_buffer, block_size, false);
	
	free(p_buffer);
	fclose(p_file);
}

static void write_map(uint32_t image_width, uint32_t image_height, uint32_t tile_width, uint32_t tile_height, uint32_t block_width, uint32_t block_height)
{
	char map_filename[256] = { 0 };
	create_filename(map_filename, m_args.out_filename, IS_COMPRESSED ? EXT_NXM_COMPRESSED : EXT_NXM);
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
	
	write_file(p_file, map_filename, p_buffer, map_size, m_args.map_16bit);
	
	free(p_buffer);
	fclose(p_file);
}

static int get_tile(int tx, int ty)
{
	m_palette_index = 0;
	
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
						m_palette_index = pix >> 4;
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
					
					if ((m_chunk_size >> 4) != 0)
					{
						m_palette_index = (pix >> 4) & 0xf;
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
	bool found = false;
	
	if (m_args.tile_repeat)
	{
		for (int i = 0; i < m_tile_count; i++)
		{
			found = true;
			
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
					found = false;
					break;
				}
			}
			
			if (found)
			{
				m_chunk_size = m_tile_size;
				tile_index = i;
				break;
			}
		}
	}
	
	if (!found)
	{
		m_tile_count++;
	}
	
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
		return get_tile(tbx, tby);
	}
	
	for (int y = 0; y < m_block_height; y++)
	{
		for (int x = 0; x < m_block_width; x++)
		{
			m_blocks[(m_block_count * m_block_width * m_block_height) + (y * m_block_width) + x] = get_tile(tbx + (x * m_tile_width), tby + (y * m_tile_height));
		}
	}
	
	uint32_t block_index = m_block_count;
	bool found = false;
	
	if (m_args.block_repeat)
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
						uint32_t ti = get_tile(x * m_tile_width, y * m_tile_height);
						
						if (m_args.colors_4bit)
						{
							m_map[x * (m_image_height / m_tile_height) + y] = (ti & 0x3fff) + (m_palette_index << 12);
						}
						else
						{
							m_map[x * (m_image_height / m_tile_height) + y] = ti;
						}
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
						uint32_t ti = get_tile(x * m_tile_width, y * m_tile_height);
						
						if (m_args.colors_4bit)
						{
							m_map[y * (m_image_width / m_tile_width) + x] = (ti & 0x3fff) + (m_palette_index << 12);
						}
						else
						{
							m_map[y * (m_image_width / m_tile_width) + x] = ti;
						}
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
}

static void parse_tile(char *line, int first_gid, int *tile_count)
{
	char *pch = strtok(line, ",\r\n");
	
	while (pch != NULL)
	{
		int tile_id = atoi(pch);
		
		if (tile_id == -1)
			tile_id = m_args.tiled_blank;
		
		m_map[(*tile_count)++] = tile_id - first_gid;
		
		pch = strtok(NULL, ",\r\n");
	}
}

/* static void parse_csv(char *file_name, int *tile_count)
{
	char line[512];
	FILE *csv_file = fopen(file_name, "r");
	
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
			strncpy(string, pch_s, pch_e - pch_s);
			
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

static void parse_tmx(char *file_name)
{
	char line[512], string[32];
	FILE *csv_file = fopen(file_name, "r");
	int map_width = 0, map_height = 0;
	int tile_width = 0, tile_height = 0;
	int tile_count = 0;
	int first_gid = 0;
	bool is_data = false;

	while (fgets(line, 512, csv_file))
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
			
			continue;
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
	
	int image_width = map_width * tile_width;
	int image_height = map_height * tile_height;

	if (!m_args.map_none)
	{		
		write_map(image_width, image_height, tile_width, tile_height, 1, 1);
	}
}

int main(int argc, char *argv[])
{
	atexit(exit_handler);

	// Parse program arguments.
	if (!parse_args(argc, argv, &m_args))
	{
		exit(EXIT_FAILURE);
	}

	// Create file names for raw image file and, if separate, raw palette file.
	create_filename(m_bitmap_filename, m_args.out_filename != NULL ? m_args.out_filename : m_args.in_filename, IS_COMPRESSED ? EXT_NXI_COMPRESSED : EXT_NXI);
	
	if (!strcmp(m_args.in_filename, m_bitmap_filename))
	{
		exit_with_msg("Input file and output file cannot have the same name (%s == %s).\n", m_args.in_filename, m_bitmap_filename);
	}
	
	const char *p_in_filename = (const char *)m_args.in_filename;
	
	if ((p_in_filename = strrchr(m_args.in_filename,'.')) != NULL)
	{
		if (strcasecmp(p_in_filename, ".png") == 0)
		{
			read_png();
		}
		else
		{
			read_bitmap();
		}
	}
	
	if (m_args.asm_mode > ASMMODE_NONE)
	{
		char asm_filename[256] = { 0 };
		
		create_filename(asm_filename, m_args.in_filename, ".asm");
		
		m_asm_file = fopen(asm_filename, "w");
		
		if (asm_filename == NULL)
		{
			exit_with_msg("Can't create asm file %s.\n", asm_filename);
		}
		
		write_asm_header();
		
		if (m_args.asm_mode == ASMMODE_Z80ASM)
		{
			char header_filename[256] = { 0 };
			
			create_filename(header_filename, m_args.in_filename, ".h");
			
			m_header_file = fopen(header_filename, "w");
			
			if (header_filename == NULL)
			{
				exit_with_msg("Can't create header file %s.\n", header_filename);
			}

			write_header_header();
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
	
	process_palette();
	
	if (m_args.pal_file != NULL)
	{
		read_file(m_args.pal_file, (uint8_t *)m_next_palette, sizeof(m_next_palette));
	}
	else
	{
		create_next_palette(m_args.color_mode);
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
			write_header_footer();
		}
		
		exit(EXIT_SUCCESS);
	}

	process_tiles();
	
	write_next_palette();
	
	if (m_args.tiled_file != NULL)
	{
		parse_tmx(m_args.tiled_file);
	}
	else if (!m_args.map_none)
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
		else
		{
			
			if (m_block_count > 0)
			{
				printf("Block Count = %d\n", m_block_count);
				
				write_blocks();
			}
			
			printf("Tile Count = %d\n", m_tile_count);
				
			write_tiles_sprites();
		}
	}
	
	if (m_args.asm_mode == ASMMODE_Z80ASM)
	{
		write_header_footer();
	}
	
	return 0;
}
