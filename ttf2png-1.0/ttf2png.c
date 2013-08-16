/*
ttf2png - True Type Font to PNG converter
Copyright (c) 2004-2008 Mikko Rasa

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct sImage
{
	unsigned w, h;
	char     *data;
} Image;

typedef struct sGlyph
{
	unsigned index;
	unsigned code;
	Image    image;
	unsigned x, y;
	int      offset_x;
	int      offset_y;
	int      advance;
} Glyph;

typedef struct sKerning
{
	unsigned left_code;
	unsigned right_code;
	int distance;
} Kerning;

typedef struct sFont
{
	unsigned size;
	int      ascent;
	int      descent;
	unsigned n_glyphs;
	Glyph    *glyphs;
	unsigned n_kerning;
	Kerning  *kerning;
	Image    image;
} Font;

void usage();
unsigned round_to_pot(unsigned);
void *alloc_image_data(size_t, size_t);
int init_font(Font *, FT_Face, unsigned, unsigned, int);
int render_grid(Font *, unsigned, unsigned, unsigned, int);
int render_packed(Font *);
int save_defs(const char *, const Font *);
int save_png(const char *, const Image *, char);

char verbose = 0;

int main(int argc, char **argv)
{
	char *fn;
	int  begin = 0;
	int  end = 255;
	int  size = 10;
	int  cpl = 0;
	int  cellw = 0;
	int  cellh = 0;
	char autohinter = 0;
	char seq = 0;
	char alpha = 0;
	char invert = 0;
	char pack = 0;

	FT_Library freetype;
	FT_Face    face;

	int  err;
	int  i;

	char *out_fn = "font.png";
	char *def_fn = NULL;

	Font font;

	if(argc<2)
	{
		usage();
		return 1;
	}

	while((i = getopt(argc, argv, "r:s:l:c:o:atvh?ed:pi")) != -1)
	{
		char *ptr;
		int  temp;
		switch(i)
		{
		case 'r':
			if(!strcmp(optarg, "all"))
			{
				begin = 0;
				end = 0x110000;
			}
			else
			{
				if(!isdigit(optarg[0]))
					temp = -1;
				else
				{
					temp = strtol(optarg, &ptr, 0);
					if(ptr[0]!=',' || !isdigit(ptr[1]))
						temp = -1;
				}
				if(temp<0)
				{
					printf("Not a valid range: %s\n", optarg);
					exit(1);
				}
				else
				{
					begin = temp;
					end = strtol(ptr+1, NULL, 0);
				}
			}
			break;
		case 's':
			size = strtol(optarg, NULL, 0);
			break;
		case 'l':
			cpl = strtol(optarg, NULL, 0);
			break;
		case 'c':
			if(!strcmp(optarg, "auto"))
			{
				cellw = 0;
				cellh = 0;
			}
			else if(!strcmp(optarg, "autorect"))
			{
				cellw = 0;
				cellh = 1;
			}
			else
			{
				cellw = strtol(optarg, &ptr, 0);
				if(ptr[0]=='x' && isdigit(ptr[1]))
					cellh = strtol(ptr+1, NULL, 0);
				else
					cellh = cellw;
			}
			break;
		case 'o':
			out_fn = optarg;
			break;
		case 'a':
			autohinter = 1;
			break;
		case 't':
			alpha = 1;
			break;
		case 'v':
			++verbose;
			break;
		case 'h':
		case '?':
			usage();
			return 0;
		case 'e':
			seq = 1;
			break;
		case 'd':
			def_fn = optarg;
			break;
		case 'p':
			pack = 1;
			break;
		case 'i':
			invert = 1;
			break;
		}
	}
	if(!strcmp(out_fn, "-"))
		verbose = 0;

	if(optind!=argc-1)
	{
		usage();
		return 1;
	}

	fn = argv[optind];

	err = FT_Init_FreeType(&freetype);
	if(err)
	{
		fprintf(stderr, "Couldn't initialize FreeType library\n");
		return 1;
	}

	err = FT_New_Face(freetype, fn, 0, &face);
	if(err)
	{
		fprintf(stderr, "Couldn't load font file\n");
		if(err==FT_Err_Unknown_File_Format)
			fprintf(stderr, "Unknown file format\n");
		return 1;
	}

	if(verbose)
	{
		const char *name = FT_Get_Postscript_Name(face);
		printf("Font name: %s\n", name);
		printf("Glyphs:    %ld\n", face->num_glyphs);
	}

	err = FT_Set_Pixel_Sizes(face, 0, size);
	if(err)
	{
		fprintf(stderr, "Couldn't set size\n");
		return 1;
	}

	font.size = size;
	err = init_font(&font, face, begin, end, autohinter);
	if(err)
		return 1;

	if(pack)
		err = render_packed(&font);
	else
		err = render_grid(&font, cellw, cellh, cpl, seq);
	if(err)
		return 1;

	if(invert)
	{
		for(i=0; (unsigned)i<font.image.w*font.image.h; ++i)
			font.image.data[i] = 255-font.image.data[i];
	}
	err = save_png(out_fn, &font.image, alpha);
	if(err)
		return 1;

	if(def_fn)
		save_defs(def_fn, &font);

	for(i=0; (unsigned)i<font.n_glyphs; ++i)
		free(font.glyphs[i].image.data);
	free(font.glyphs);
	free(font.kerning);
	free(font.image.data);

	FT_Done_Face(face);
	FT_Done_FreeType(freetype);

	return 0;
}

void usage()
{
	printf("ttf2png 1.0 - True Type Font to PNG converter\n"
		"Copyright (c) 2004-2008  Mikko Rasa, Mikkosoft Productions\n"
		"Distributed under the GNU General Public License\n\n");

	printf("Usage: ttf2png [options] <TTF file>\n\n");

	printf("Accepted options (default values in [brackets])\n"
		"  -r  Range of characters to convert [0,255]\n"
		"  -s  Font size to use, in pixels [10]\n"
		"  -l  Number of characters to put in one line [auto]\n"
		"  -c  Character cell size, in pixels [auto]\n"
		"  -o  Output file name (or - for stdout) [font.png]\n");
	printf("  -a  Force autohinter\n"
		"  -t  Render glyphs to alpha channel\n"
		"  -i  Invert colors of the glyphs\n"
		"  -v  Increase the level of verbosity\n"
		"  -e  Use cells in sequence, without gaps\n"
		"  -p  Pack the glyphs tightly instead of in a grid\n"
		"  -d  File name for writing glyph definitions\n"
		"  -h  Print this message\n");
}

unsigned round_to_pot(unsigned n)
{
	n -= 1;
	n |= n>>1;
	n |= n>>2;
	n |= n>>4;
	n |= n>>8;
	n |= n>>16;

	return n+1;
}

void *alloc_image_data(size_t a, size_t b)
{
	void *ptr;

	/* Carry out the multiplication manually so we can check for overflow. */
	while(b>1)
	{
		size_t c = a;
		a *= 2;
		if(b&1)
			a += c;
		if(a<c)
		{
			fprintf(stderr, "Cannot allocate %lu kbytes of memory for image\n", (unsigned long)(c/1024*b));
			return NULL;
		}
		b /= 2;
	}
	ptr = malloc(a);
	if(!ptr)
		fprintf(stderr, "Cannot allocate %lu kbytes of memory for image\n", (unsigned long)(a/1024*b));
	return ptr;
}

int init_font(Font *font, FT_Face face, unsigned first, unsigned last, int autohinter)
{
	unsigned i, j;
	unsigned size = 0;

	font->ascent = (face->size->metrics.ascender+63)>>6;
	font->descent = (face->size->metrics.descender+63)>>6;

	if(verbose>=1)
	{
		printf("Ascent:    %d\n", font->ascent);
		printf("Descent:   %d\n", font->descent);
	}

	font->n_glyphs = 0;
	font->glyphs = NULL;
	for(i=first; i<=last; ++i)
	{
		unsigned  n;
		FT_Bitmap *bmp = &face->glyph->bitmap;
		int       x, y;
		int       flags = 0;
		Glyph     *glyph;

		n = FT_Get_Char_Index(face, i);
		if(!n)
			continue;

		if(autohinter)
			flags |= FT_LOAD_FORCE_AUTOHINT;
		FT_Load_Glyph(face, n, flags);
		FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

		if(verbose>=2)
			printf("  Char %u: glyph %u, size %dx%d\n", i, n, bmp->width, bmp->rows);

		if(bmp->pixel_mode!=FT_PIXEL_MODE_GRAY)
		{
			fprintf(stderr, "Warning: Glyph %u skipped, not grayscale\n", n);
			continue;
		}

		if(font->n_glyphs>=size)
		{
			size += 16;
			font->glyphs = (Glyph *)realloc(font->glyphs, size*sizeof(Glyph));
		}

		glyph = &font->glyphs[font->n_glyphs++];
		glyph->index = n;
		glyph->code = i;
		glyph->image.w = bmp->width;
		glyph->image.h = bmp->rows;
		glyph->image.data = (char *)malloc(bmp->width*bmp->rows);
		if(!glyph->image.data)
		{
			fprintf(stderr, "Cannot allocate %d bytes of memory for glyph\n", bmp->width*bmp->rows);
			return -1;
		}
		glyph->offset_x = face->glyph->bitmap_left;
		glyph->offset_y = face->glyph->bitmap_top-bmp->rows;
		glyph->advance = (int)(face->glyph->advance.x+32)/64;

		/* Copy the glyph image since FreeType uses a global buffer, which would
		be overwritten by the next glyph.  Negative pitch means the scanlines
		start from the bottom. */
		if(bmp->pitch<0)
		{
			for(y=0; y<bmp->rows; ++y) for(x=0; x<bmp->width; ++x)
				glyph->image.data[x+(glyph->image.h-1-y)*glyph->image.w] = bmp->buffer[x-y*bmp->pitch];
		}
		else
		{
			for(y=0; y<bmp->rows; ++y) for(x=0; x<bmp->width; ++x)
				glyph->image.data[x+y*glyph->image.w] = bmp->buffer[x+y*bmp->pitch];
		}
	}

	if(verbose>=1)
		printf("Loaded %u glyphs\n", font->n_glyphs);

	size = 0;
	font->n_kerning = 0;
	font->kerning = NULL;
	for(i=0; i<font->n_glyphs; ++i) for(j=0; j<font->n_glyphs; ++j)
		if(j!=i)
		{
			FT_Vector kerning;
			FT_Get_Kerning(face, font->glyphs[i].index, font->glyphs[j].index, FT_KERNING_DEFAULT, &kerning);

			/* FreeType documentation says that vertical kerning is practically
			never used, so we ignore it. */
			if(kerning.x)
			{
				Kerning *kern;

				if(font->n_kerning>=size)
				{
					size += 16;
					font->kerning = (Kerning *)realloc(font->kerning, size*sizeof(Kerning));
				}

				kern = &font->kerning[font->n_kerning++];
				kern->left_code = font->glyphs[i].code;
				kern->right_code = font->glyphs[j].code;
				kern->distance = kerning.x/64;
			}
		}

	if(verbose>=1)
		printf("Loaded %d kerning pairs\n", font->n_kerning);

	return 0;
}

int render_grid(Font *font, unsigned cellw, unsigned cellh, unsigned cpl, int seq)
{
	unsigned i;
	int      top = 0, bot = 0;
	unsigned first, last;
	unsigned maxw = 0, maxh = 0;

	/* Find extremes of the glyph images. */
	for(i=0; i<font->n_glyphs; ++i)
	{
		int y;

		y = font->glyphs[i].offset_y+font->glyphs[i].image.h;
		if(y>top)
			top = y;
		if(font->glyphs[i].offset_y<bot)
			bot = font->glyphs[i].offset_y;
		if(font->glyphs[i].image.w>maxw)
			maxw = font->glyphs[i].image.w;
		if(font->glyphs[i].image.h>maxh)
			maxh = font->glyphs[i].image.h;
	}

	if(cellw==0)
	{
		/* Establish a large enough cell to hold all glyphs in the range. */
		int square = (cellh==cellw);
		cellw = maxw;
		cellh = top-bot;
		if(square)
		{
			if(cellh>cellw)
				cellw = cellh;
			else
				cellh = cellw;
		}
	}

	if(verbose>=1)
	{
		printf("Max size:  %u x %u\n", maxw, maxh);
		printf("Y range:   [%d %d]\n", bot, top);
		printf("Cell size: %u x %u\n", cellw, cellh);
		if(maxw>cellw || (unsigned)(top-bot)>cellh)
			fprintf(stderr, "Warning: character size exceeds cell size\n");
	}

	if(cpl==0)
	{
		/* Determine number of characters per line, trying to fit all the glyphs
		in a square image. */
		for(i=1;; i<<=1)
		{
			cpl = i/cellw;
			if(cpl>0 && font->n_glyphs/cpl*cellh<=cpl*cellw)
				break;
		}
	}

	first = font->glyphs[0].code;
	if(!seq)
		first -= first%cpl;
	last = font->glyphs[font->n_glyphs-1].code;

	font->image.w = round_to_pot(cpl*cellw);
	font->image.h = round_to_pot((last-first+cpl)/cpl*cellh);

	font->image.data = (char *)alloc_image_data(font->image.w, font->image.h);
	if(!font->image.data)
		return -1;
	memset(font->image.data, 255, font->image.w*font->image.h);

	for(i=0; i<font->n_glyphs; ++i)
	{
		Glyph    *glyph;
		unsigned ci, cx, cy;
		unsigned x, y;

		glyph = &font->glyphs[i];

		if(seq)
			ci = i;
		else
			ci = glyph->code-first;

		cx = (ci%cpl)*cellw;
		cy = (ci/cpl)*cellh;

		if(cellw>glyph->image.w)
			cx += (cellw-glyph->image.w)/2;
		cy += top-glyph->offset_y-glyph->image.h;

		glyph->x = cx;
		glyph->y = cy;

		for(y=0; y<glyph->image.h; ++y) for(x=0; x<glyph->image.w; ++x)
		{
			if(cx+x>=font->image.w || cy+y>=font->image.h)
				continue;
			font->image.data[cx+x+(cy+y)*font->image.w] = 255-glyph->image.data[x+y*glyph->image.w];
		}
	}

	return 0;
}

int render_packed(Font *font)
{
	unsigned i;
	size_t   area = 0;
	char     *used_glyphs;
	unsigned *used_pixels;
	unsigned cx = 0, cy;
	unsigned used_h = 0;

	/* Compute the total area occupied by glyphs and padding. */
	for(i=0; i<font->n_glyphs; ++i)
	{
		size_t a = area+(font->glyphs[i].image.w+1)*(font->glyphs[i].image.h+1);
		if(a<area)
		{
			fprintf(stderr, "Overflow in counting total glyph area\n");
			return -1;
		}
		area = a;
	}

	/* Find an image size that's no higher than wide, allowing for some
	imperfections in the packing. */
	for(font->image.w=1;; font->image.w<<=1)
	{
		font->image.h = (area*5/4)/font->image.w;
		if(font->image.h<=font->image.w)
			break;
	}
	font->image.h = round_to_pot(font->image.h);

	/* Allocate arrays for storing the image and keeping track of used pixels and
	glyphs.  Since glyphs are rectangular and the image is filled starting from
	the top, it's enough to track the number of used pixels at the top of each
	column. */
	font->image.data = (char *)alloc_image_data(font->image.w, font->image.h);
	if(!font->image.data)
		return -1;
	memset(font->image.data, 255, font->image.w*font->image.h);
	used_pixels = (unsigned *)malloc(font->image.w*sizeof(unsigned));
	memset(used_pixels, 0, font->image.w*sizeof(unsigned));
	used_glyphs = (char *)malloc(font->n_glyphs);
	memset(used_glyphs, 0, font->n_glyphs);

	for(cy=0; cy<font->image.h;)
	{
		unsigned w;
		unsigned x, y;
		Glyph    *glyph = NULL;
		unsigned best_score = 0;
		unsigned target_h = 0;

		/* Find the leftmost free pixel on this row.    Also record the lowest extent of glyphs
		to the left of the free position. */
		for(; (cx<font->image.w && used_pixels[cx]>cy); ++cx)
			if(used_pixels[cx]-cy-1>target_h)
				target_h = used_pixels[cx]-cy-1;

		if(cx>=font->image.w)
		{
			cx = 0;
			++cy;
			continue;
		}

		/* Count the free pixel at this position. */
		for(w=0; (cx+w<font->image.w && used_pixels[cx+w]<=cy); ++w) ;

		/* Find a suitable glyph to put here. */
		for(i=0; i<font->n_glyphs; ++i)
		{
			Glyph *g;

			g = &font->glyphs[i];
			if(!used_glyphs[i] && g->image.w<=w)
			{
				unsigned score;

				/* Prefer glyphs that would reach exactly as low as the ones left
				of here.  This aims to create a straight edge at the bottom for
				lining up further glyphs. */
				score = g->image.h+1;
				if(g->image.h==target_h)
					score *= g->image.w;
				else
					score += g->image.w;

				if(score>best_score)
				{
					glyph = g;
					best_score = score;
				}
			}
		}

		if(!glyph)
		{
			cx += w;
			continue;
		}

		used_glyphs[glyph-font->glyphs] = 1;
		glyph->x = cx;
		glyph->y = cy;

		for(y=0; y<glyph->image.h; ++y) for(x=0; x<glyph->image.w; ++x)
		{
			if(cx+x>=font->image.w || cy+y>=font->image.h)
				continue;
			font->image.data[cx+x+(cy+y)*font->image.w] = 255-glyph->image.data[x+y*glyph->image.w];
		}
		for(x=0; x<glyph->image.w+2; ++x)
		{
			if(cx+x<1 || cx+x>font->image.w)
				continue;
			if(used_pixels[cx+x-1]<cy+glyph->image.h+1)
				used_pixels[cx+x-1] = cy+glyph->image.h+1;
		}

		if(cy+glyph->image.h>used_h)
			used_h = cy+glyph->image.h;
	}

	/* Trim the image to the actually used size, in case the original estimate
	was too pessimistic. */
	font->image.h = round_to_pot(used_h);

	free(used_glyphs);
	free(used_pixels);

	return 0;
}

int save_defs(const char *fn, const Font *font)
{
	FILE     *out;
	unsigned i;

	out = fopen(fn, "w");
	if(!out)
	{
		fprintf(stderr, "Couldn't open %s\n",fn);
		return -1;
	}

	fprintf(out, "# Image/font info:\n");
	fprintf(out, "# width height size ascent descent\n");
	fprintf(out, "font %d %d %d %d %d\n", font->image.w, font->image.h, font->size, font->ascent, font->descent);
	fprintf(out, "\n# Glyph info:\n");
	fprintf(out, "# code x y width height offset_x offset_y advance\n");
	for(i=0; i<font->n_glyphs; ++i)
	{
		const Glyph *g = &font->glyphs[i];
		fprintf(out, "glyph %u %u %u %u %u %d %d %d\n", g->code, g->x, g->y, g->image.w, g->image.h, g->offset_x, g->offset_y, g->advance);
	}
	fprintf(out, "\n# Kerning info:\n");
	fprintf(out, "# left right distance\n");
	for(i=0; i<font->n_kerning; ++i)
	{
		const Kerning *k = &font->kerning[i];
		fprintf(out, "kern %u %u %d\n", k->left_code, k->right_code, k->distance);
	}

	fclose(out);

	return 0;
}

int save_png(const char *fn, const Image *image, char alpha)
{
	FILE       *out;
	png_struct *pngs;
	png_info   *pngi;
	png_byte   **rows;
	unsigned   i;
	png_byte   *data2 = 0;
	int        color;

	if(!strcmp(fn, "-"))
		out = stdout;
	else
	{
		out = fopen(fn, "wb");
		if(!out)
		{
			fprintf(stderr, "Couldn't open %s\n",fn);
			return -1;
		}
	}

	pngs = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!pngs)
	{
		fprintf(stderr, "Error writing PNG file\n");
		return -1;
	}
	pngi = png_create_info_struct(pngs);
	if(!pngi)
	{
		png_destroy_write_struct(&pngs, NULL);
		fprintf(stderr, "Error writing PNG file\n");
		return -1;
	}

	png_init_io(pngs, out);
	rows = (png_byte **)malloc(image->h*sizeof(png_byte *));
	if(alpha)
	{
		data2 = (png_byte *)alloc_image_data(image->w*2, image->h);
		if(!data2)
			return -1;
		for(i=0; i<image->w*image->h; ++i)
		{
			data2[i*2] = 255;
			data2[i*2+1] = 255-image->data[i];
		}
		for(i=0; i<image->h; ++i)
			rows[i] = (png_byte *)(data2+i*image->w*2);
		color = PNG_COLOR_TYPE_GRAY_ALPHA;
	}
	else
	{
		for(i=0; i<image->h; ++i)
			rows[i] = (png_byte *)(image->data+i*image->w);
		color = PNG_COLOR_TYPE_GRAY;
	}
	png_set_IHDR(pngs, pngi, image->w, image->h, 8, color, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_rows(pngs, pngi, rows);
	png_write_png(pngs, pngi, PNG_TRANSFORM_IDENTITY, NULL);
	png_destroy_write_struct(&pngs, &pngi);
	free(rows);
	if(alpha)
		free(data2);

	if(verbose)
		printf("Saved %dx%d PNG image to %s\n", image->w, image->h, fn);

	fclose(out);

	return 0;
}
