// SPDX-License-Identifier: WTFPL
//
// Copyright 2022 Matt
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See the COPYING file for more details.

// fucking garbage file format

#define ffs(x)     __builtin_ffs(x)
#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)

#define FONT_NOOVERLAP    (1 << 0)
#define FONT_CONSTMETRICS (1 << 1)
#define FONT_TERMFONT     (1 << 2)
#define FONT_CONSTWIDTH   (1 << 3)
#define FONT_INKINSIDE    (1 << 4)
#define FONT_INKMETRICS   (1 << 5)
#define FONT_RIGHTTOLEFT  (1 << 6)

struct font {
	int fd;
	u8  flag;               // FONT_* bits
	u16 width;
	u16 height;
	u16 cacheminchar;       // codepoint represented by idxcache[0]
	u16 cacheminindex;
	u32 firstcodepoint;
	u32 lastcodepoint;

	/// dev cache stats
	u32 nlookup;
	u32 nmiss;

	u32 encodingformat;
	u32 glyphindexoff;      // file offset of pcfencodingtab.glyphindex
	u16 idxcache[64];       // (POWER OF 2) glyphindex for codepoint-cacheminchar

	u32 bitmapformat;       // format member from pcfbitmaptab
	u32 bitmapoffsetoff;    // file offset of pcfbitmaptab.offset
	u32 bitmapdataoff;      // file offset of pcfbitmapdat.data
	u32 offsetcache[64]; }; // (POWER OF 2)

#define PCF_PROPERTIES       0x0001
#define PCF_ACCELERATORS     0x0002
#define PCF_METRICS          0x0004
#define PCF_BITMAPS          0x0008
#define PCF_INK_METRICS      0x0010
#define PCF_BDF_ENCODINGS    0x0020
#define PCF_SWIDTHS          0x0040
#define PCF_GLYPH_NAMES      0x0080
#define PCF_BDF_ACCELERATORS 0x0100

enum {
	// because table types are usefully defined as a bitfield
	PCF_TABLE_INVALID,
	PCF_PROPERTIES_INDEX,
	PCF_ACCELERATORS_INDEX,
	PCF_METRICS_INDEX,
	PCF_BITMAPS_INDEX,
	PCF_INK_METRICS_INDEX,
	PCF_BDF_ENCODINGS_INDEX,
	PCF_SWIDTHS_INDEX,
	PCF_GLYPH_NAMES_INDEX,
	PCF_BDF_ACCELERATORS_INDEX,
	PCF_TABLE_COUNT };

#define PCF_DEFAULT_FORMAT     0x0000
#define PCF_INKBOUNDS          0x0200
#define PCF_ACCEL_W_INKBOUNDS  0x0100
#define PCF_COMPRESSED_METRICS 0x0100

#define PCF_GLYPH_PAD_MASK 0x0003 // bitmap row padding - 0:byte, 1:short, 2:int
#define PCF_BYTE_MASK      0x0004 // if set - big endian. except sometimes when it's little endian
#define PCF_BIT_MASK       0x0008 // fuck off and die
#define PCF_SCAN_UNIT_MASK 0x0030 // bitmap data format - 0:byte, 1:short, 2:int

struct pcfheader {
	u32 magic;
	i32 ntable;
	struct pcfmetatab {
		i32 type;
		i32 format;
		i32 size;
		i32 offset; } table[]; };

struct pcfbitmaptab {
	i32 format;
	i32 nglyph;
	i32 offset[]; };

struct pcfbitmapdat {
	i32 size[4];
	u8  data[ ]; };

struct pcfencodingtab {
	i32 format;
	i16 colfirst;
	i16 collast;
	i16 rowfirst;
	i16 rowlast;
	i16 defaultchar;
	i16 glyphindex[]; };

struct pcfmetricdat {
	u8 leftbearing;
	u8 rightbearing;
	u8 width;
	u8 ascent;
	u8 descent; };

struct pcfmetricdat2 {
	i16 leftbearing;
	i16 rightbearing;
	i16 width;
	i16 ascent;
	i16 descent;
	u16 attrs; };

struct pcfmetrictab {
	i32 format;
	union {
		struct {
			i16    nmetric;
			struct pcfmetricdat metric [0]; };

		struct {
			i32    nmetric2;
			struct pcfmetricdat2 metric2[0]; }; }; };

struct pcfacceltab {
	i32 format;
	u8  nooverlap;
	u8  constmetrics;
	u8  termfont;
	u8  constwidth;
	u8  inkinside;
	u8  inkmetrics;
	u8  drawdir;
	u8  _;
	i32 fontascent;
	i32 fontdescent;
	i32 maxoverlap;
	struct pcfmetricdat2 boundsmin;
	struct pcfmetricdat2 boundsmax;
	struct pcfmetricdat2 inkboundsmin;
	struct pcfmetricdat2 inkboundsmax; };

static int
pcfinit (int fd, struct font *font) {
	int got;
	int have;
	union {
		struct pcfheader      hdr;
		struct pcfacceltab    accel;
		struct pcfencodingtab enc;
		struct pcfbitmaptab   bitmap;

		// XXX: there are 9 types of table defined. i'm choosing to believe
		// that this means we'll never see more than 9 tables in a file.
		u8 buf[sizeof (struct pcfheader) + sizeof (struct pcfmetatab)*9]; } f;

	memset (font, 0, sizeof (*font));

	font->fd = fd;

	got = pread (fd, f.buf, sizeof (f.buf), 0);
	if (got > 0) {
		have = f.hdr.ntable * sizeof (struct pcfmetatab) + sizeof (struct pcfheader);
		if (got >= have) {
			u32 taboff[PCF_TABLE_COUNT] = { };

			for (i32 i = 0; i < min (f.hdr.ntable, elements (taboff)); ++i)
				taboff[ffs(f.hdr.table[i].type)] = f.hdr.table[i].offset;

			if (taboff[PCF_ACCELERATORS_INDEX] > 0) {
				u32 tab = PCF_ACCELERATORS_INDEX;

				if (taboff[PCF_BDF_ACCELERATORS_INDEX] > 0)
					tab = PCF_BDF_ACCELERATORS_INDEX;

				// all the bdf_accelerators i've seen have a supposed
				// size of 100 but none of them actually are...
				got = pread (fd, f.buf, sizeof (struct pcfacceltab), taboff[tab]);
				if (got == sizeof (struct pcfacceltab)) {
					font->flag |= ~(--f.accel.nooverlap   ) & FONT_NOOVERLAP;
					font->flag |= ~(--f.accel.constmetrics) & FONT_CONSTMETRICS;
					font->flag |= ~(--f.accel.termfont    ) & FONT_TERMFONT;
					font->flag |= ~(--f.accel.constwidth  ) & FONT_CONSTWIDTH;
					font->flag |= ~(--f.accel.inkinside   ) & FONT_INKINSIDE;
					font->flag |= ~(--f.accel.inkmetrics  ) & FONT_INKMETRICS;
					font->flag |= ~(--f.accel.drawdir     ) & FONT_RIGHTTOLEFT;

					font->width  = f.accel.boundsmax.width;
					font->height = f.accel.boundsmax.ascent + f.accel.boundsmax.descent;
					if (f.accel.format & PCF_BYTE_MASK) {
						font->width  = bswap16 (font->width);
						font->height = bswap16 (font->height); }}}

			font->cacheminchar = ~0;
			if (taboff[PCF_BDF_ENCODINGS_INDEX] > 0) {
				got = pread (fd, f.buf, sizeof (struct pcfencodingtab), taboff[PCF_BDF_ENCODINGS_INDEX]);
				if (got == sizeof (struct pcfencodingtab)) {
					u16 cf = f.enc.colfirst;
					u16 cl = f.enc.collast;
					u16 rf = f.enc.rowfirst;
					u16 rl = f.enc.rowlast;
					if (f.enc.format & PCF_BYTE_MASK) {
						cf = bswap16 (cf);
						cl = bswap16 (cl);
						rf = bswap16 (rf);
						rl = bswap16 (rl); }

					font->firstcodepoint = rf * (cl - cf + 1) + cf;
					font->lastcodepoint  = rl * (cl - cf + 1) + (cl - cf + 1);

//					printf ("cf %hu cl %hu rf %hu rl %hu first %hu last %hu\n", cf, cl, rf, rl, font->firstcodepoint, font->lastcodepoint);

					font->encodingformat = f.enc.format;
					font->glyphindexoff  = taboff[PCF_BDF_ENCODINGS_INDEX] + offsetof (struct pcfencodingtab, glyphindex);
//					printf ("glyphindexoff: 0x%04x, firstcodepoint: %u, lastcodepoint: %u\n", font->glyphindexoff, font->firstcodepoint, font->lastcodepoint);
					}}

			if (taboff[PCF_BITMAPS_INDEX] > 0) {
				got = pread (fd, f.buf, sizeof (struct pcfbitmaptab), taboff[PCF_BITMAPS_INDEX]);
				if (got == sizeof (struct pcfbitmaptab)) {
					i32 nglyph = f.bitmap.nglyph;
					if (f.bitmap.format & PCF_BYTE_MASK)
						nglyph = bswap32 (nglyph);

					font->bitmapformat    = f.bitmap.format;
					font->bitmapoffsetoff = taboff[PCF_BITMAPS_INDEX] + offsetof (struct pcfbitmaptab, offset);
					font->bitmapdataoff   = font->bitmapoffsetoff + (nglyph * sizeof (f.bitmap.offset[0])) + offsetof (struct pcfbitmapdat, data);
//					printf ("offsetoff: 0x%04x, dataoff: 0x%04x, nglyph: %u\n", font->bitmapoffsetoff, font->bitmapdataoff, nglyph);
					}}

			return fd; }}

	return -1; }

static noinline int
__pcfloadcache (struct font *font, u32 cp) {
	u32 mincp     = cp & ~((elements (font->idxcache)/2) - 1);
	/// BUG: indexoff only correct when mincp == 0
	u32 indexoff  = font->glyphindexoff + mincp * sizeof (font->idxcache[0]);
	u16 minindex  = ~0;
	u16 maxindex  =  0;
	u32 bitmapoff;
	int nb;

	nb  = 0;
	nb |= pread (font->fd, font->idxcache, sizeof (font->idxcache), indexoff);
	for (int i = 0; i < elements (font->idxcache); ++i) {
		u16 idx = font->idxcache[i];
		if (idx != 0xffff) {
			if (font->encodingformat & PCF_BYTE_MASK)
				idx = bswap16 (idx);

			if (idx < minindex) minindex = idx;
			if (idx > maxindex) maxindex = idx;

			font->idxcache[i] = idx; }}

//	printf ("minindex: %hu, maxindex: %hu, delta: %hu\n", minindex, maxindex, maxindex-minindex);
	if (maxindex - minindex > elements (font->offsetcache)) {
		write (2, sl ("ERROR: index range is greater than the size of the cache and fuck i dont know!\n"));
		exit  (99); }

	bitmapoff = font->bitmapoffsetoff + minindex * sizeof (font->offsetcache[0]);
	nb |= pread (font->fd, font->offsetcache, sizeof (font->offsetcache), bitmapoff);
	if (nb <= 0)
		return -2;

	font->cacheminindex = minindex;
	font->cacheminchar  = mincp;

	// BUG: indexoff (prob bitmapoff too) is totally wrong in multirow fonts
//	printf ("indexoffset: 0x%04x, bitmapoffset: 0x%04x\n", indexoff, bitmapoff);

	font->nmiss++;
	return 0; }

static int
pcfglyph (struct font *font, u32 cp, u8 *pixel) {
	int nb;
	u32 glyphidx;
	u32 bmoffset;
	u32 bitstore;
	u32 rowpad;
	u32 bmsz;
	u8* bitmap;

	if (cp < font->firstcodepoint || cp > font->lastcodepoint)
		return -1;

	font->nlookup++;
	if (cp < font->cacheminchar || cp >= font->cacheminchar + elements (font->idxcache))
		__pcfloadcache (font, cp);

	glyphidx = font->idxcache[cp - font->cacheminchar];
	if (glyphidx != 0xffff) {
		glyphidx -= font->cacheminindex;
		bmoffset  = font->offsetcache[glyphidx];
		if (font->bitmapformat & PCF_BYTE_MASK)
			bmoffset = bswap32 (bmoffset);

		bmoffset += font->bitmapdataoff;

		rowpad   = 1 << (font->bitmapformat & PCF_GLYPH_PAD_MASK);
		bitstore = (font->bitmapformat & PCF_SCAN_UNIT_MASK) >> 4; // TODO: dunno what to do with this

		// BUG: width is actually measured in bits
		bmsz   = font->height * (font->width + (rowpad-1) & ~(rowpad-1)); // TODO: this "massively" over allocates
		bitmap = alloca (bmsz);

		nb = pread (font->fd, bitmap, bmsz, bmoffset);
		if (nb > 0) {
			u32 rowbits;
			u8* row;

			rowpad  = rowpad * 8 - 1;
			rowbits = (font->width + rowpad & ~rowpad);
			if (rowbits > 32)
				return -2;

			row = alloca (rowbits);
			for (u32 y = 0; y < font->height; ++y) {
				for (u32 x = 0; x < rowbits/8; ++x) {
					u8 byte = *bitmap++;
					for (u32 b = 0; b < 8; ++b) {
						u32 i = x*8+b;

						row[i] = 0;
						if (byte & 128)
							row[i] = ~0;

						byte <<= 1; }}

				memcpy (pixel, row, font->width);
				pixel += font->width; }

			return 0; }}

	return -1; }
