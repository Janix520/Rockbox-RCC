/*
 * Convert BDF files to C source and/or Rockbox .fnt file format
 *
 * Copyright (c) 2002 by Greg Haerr <greg@censoft.com>
 *
 * What fun it is converting font data...
 *
 * 09/17/02	Version 1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROTATE /* define this for the new, rotated format */

/* BEGIN font.h*/
/* loadable font magic and version #*/
#ifdef ROTATE
#define VERSION		"RB12" /* newer version */
#else
#define VERSION		"RB11"
#endif

/* bitmap_t helper macros*/
#define BITMAP_WORDS(x)         (((x)+15)/16)	/* image size in words*/
#define BITMAP_BYTES(x)         (BITMAP_WORDS(x)*sizeof(bitmap_t))
#define	BITMAP_BITSPERIMAGE     (sizeof(bitmap_t) * 8)
#define	BITMAP_BITVALUE(n)      ((bitmap_t) (((bitmap_t) 1) << (n)))
#define	BITMAP_FIRSTBIT         (BITMAP_BITVALUE(BITMAP_BITSPERIMAGE - 1))
#define	BITMAP_TESTBIT(m)	((m) & BITMAP_FIRSTBIT)
#define	BITMAP_SHIFTBIT(m)	((bitmap_t) ((m) << 1))

typedef unsigned short bitmap_t; /* bitmap image unit size*/

/* builtin C-based proportional/fixed font structure */
/* based on The Microwindows Project http://microwindows.org */
struct font {
    int		maxwidth;	/* max width in pixels*/
    int 	height;		/* height in pixels*/
    int		ascent;		/* ascent (baseline) height*/
    int		firstchar;	/* first character in bitmap*/
    int		size;		/* font size in glyphs*/
    bitmap_t*	bits;		/* 16-bit right-padded bitmap data*/
    unsigned long* offset;	/* offsets into bitmap data*/
    unsigned char* width;	/* character widths or NULL if fixed*/
    int		defaultchar;	/* default char (not glyph index)*/
    long	bits_size;	/* # words of bitmap_t bits*/
    
    /* unused by runtime system, read in by convbdf*/
    unsigned long* offrot;	/* offsets into rotated bitmap data*/
    char *	name;		/* font name*/
    char *	facename;	/* facename of font*/
    char *	copyright;	/* copyright info for loadable fonts*/
    int		pixel_size;
    int		descent;
    int		fbbw, fbbh, fbbx, fbby;
};
/* END font.h*/

#define isprefix(buf,str)	(!strncmp(buf, str, strlen(str)))
#define	strequal(s1,s2)		(!strcmp(s1, s2))

#define EXTRA	300		/* # bytes extra allocation for buggy .bdf files*/

int gen_c = 0;
int gen_fnt = 0;
int gen_map = 1;
int start_char = 0;
int limit_char = 65535;
int oflag = 0;
char outfile[256];

void usage(void);
void getopts(int *pac, char ***pav);
int convbdf(char *path);

void free_font(struct font* pf);
struct font* bdf_read_font(char *path);
int bdf_read_header(FILE *fp, struct font* pf);
int bdf_read_bitmaps(FILE *fp, struct font* pf);
char * bdf_getline(FILE *fp, char *buf, int len);
bitmap_t bdf_hexval(unsigned char *buf, int ndx1, int ndx2);

int gen_c_source(struct font* pf, char *path);
int gen_fnt_file(struct font* pf, char *path);

void
usage(void)
{
    char help[] = {
	"Usage: convbdf [options] [input-files]\n"
	"       convbdf [options] [-o output-file] [single-input-file]\n"
	"Options:\n"
	"    -c     Convert .bdf to .c source file\n"
	"    -f     Convert .bdf to .fnt font file\n"
	"    -s N   Start output at character encodings >= N\n"
	"    -l N   Limit output to character encodings <= N\n"
	"    -n     Don't generate bitmaps as comments in .c file\n"
	};

    fprintf(stderr, help);
}

/* parse command line options*/
void getopts(int *pac, char ***pav)
{
    char *p;
    char **av;
    int ac;
    
    ac = *pac;
    av = *pav;
    while (ac > 0 && av[0][0] == '-') {
        p = &av[0][1]; 
        while( *p)
            switch(*p++) {
            case ' ':			/* multiple -args on av[]*/
                while( *p && *p == ' ')
                    p++;
                if( *p++ != '-')	/* next option must have dash*/
                    p = "";
                break;			/* proceed to next option*/
            case 'c':			/* generate .c output*/
                gen_c = 1;
                break;
            case 'f':			/* generate .fnt output*/
                gen_fnt = 1;
                break;
            case 'n':			/* don't gen bitmap comments*/
                gen_map = 0;
                break;
            case 'o':			/* set output file*/
                oflag = 1;
                if (*p) {
                    strcpy(outfile, p);
                    while (*p && *p != ' ')
                        p++;
                }
                else {
                    av++; ac--;
                    if (ac > 0)
                        strcpy(outfile, av[0]);
                }
                break;
            case 'l':			/* set encoding limit*/
                if (*p) {
                    limit_char = atoi(p);
                    while (*p && *p != ' ')
                        p++;
                }
                else {
                    av++; ac--;
                    if (ac > 0)
                        limit_char = atoi(av[0]);
                }
                break;
            case 's':			/* set encoding start*/
                if (*p) {
                    start_char = atoi(p);
                    while (*p && *p != ' ')
                        p++;
                }
                else {
                    av++; ac--;
                    if (ac > 0)
                        start_char = atoi(av[0]);
                }
                break;
            default:
                fprintf(stderr, "Unknown option ignored: %c\r\n", *(p-1));
            }
        ++av; --ac;
    }
    *pac = ac;
    *pav = av;
}

/* remove directory prefix and file suffix from full path*/
char *basename(char *path)
{
    char *p, *b;
    static char base[256];

    /* remove prepended path and extension*/
    b = path;
    for (p=path; *p; ++p) {
        if (*p == '/')
            b = p + 1;
    }
    strcpy(base, b);
    for (p=base; *p; ++p) {
        if (*p == '.') {
            *p = 0;
            break;
        }
    }
    return base;
}

int convbdf(char *path)
{
    struct font* pf;
    int ret = 0;

    pf = bdf_read_font(path);
    if (!pf)
        exit(1);
    
    if (gen_c) {
        if (!oflag) {
            strcpy(outfile, basename(path));
            strcat(outfile, ".c");
        }
        ret |= gen_c_source(pf, outfile);
    }

    if (gen_fnt) {
        if (!oflag) {
            strcpy(outfile, basename(path));
            strcat(outfile, ".fnt");
        }
        ret |= gen_fnt_file(pf, outfile);
    }

    free_font(pf);
    return ret;
}

int main(int ac, char **av)
{
    int ret = 0;

    ++av; --ac;		/* skip av[0]*/
    getopts(&ac, &av);	/* read command line options*/

    if (ac < 1 || (!gen_c && !gen_fnt)) {
        usage();
        exit(1);
    }
    if (oflag) {
        if (ac > 1 || (gen_c && gen_fnt)) {
            usage();
            exit(1);
        }
    }
    
    while (ac > 0) {
        ret |= convbdf(av[0]);
        ++av; --ac;
    }
    
    exit(ret);
}

/* free font structure*/
void free_font(struct font* pf)
{
    if (!pf)
        return;
    if (pf->name)
        free(pf->name);
    if (pf->facename)
        free(pf->facename);
    if (pf->bits)
        free(pf->bits);
    if (pf->offset)
        free(pf->offset);
    if (pf->offrot)
        free(pf->offrot);
    if (pf->width)
        free(pf->width);
    free(pf);
}

/* build incore structure from .bdf file*/
struct font* bdf_read_font(char *path)
{
    FILE *fp;
    struct font* pf;

    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file: %s\n", path);
        return NULL;
    }
    
    pf = (struct font*)calloc(1, sizeof(struct font));
    if (!pf)
        goto errout;
	
    pf->name = strdup(basename(path));

    if (!bdf_read_header(fp, pf)) {
        fprintf(stderr, "Error reading font header\n");
        goto errout;
    }

    if (!bdf_read_bitmaps(fp, pf)) {
        fprintf(stderr, "Error reading font bitmaps\n");
        goto errout;
    }

    fclose(fp);
    return pf;

 errout:
    fclose(fp);
    free_font(pf);
    return NULL;
}

/* read bdf font header information, return 0 on error*/
int bdf_read_header(FILE *fp, struct font* pf)
{
    int encoding;
    int nchars, maxwidth;
    int firstchar = 65535;
    int lastchar = -1;
    char buf[256];
    char facename[256];
    char copyright[256];

    /* set certain values to errors for later error checking*/
    pf->defaultchar = -1;
    pf->ascent = -1;
    pf->descent = -1;

    for (;;) {
        if (!bdf_getline(fp, buf, sizeof(buf))) {
            fprintf(stderr, "Error: EOF on file\n");
            return 0;
        }
        if (isprefix(buf, "FONT ")) {		/* not required*/
            if (sscanf(buf, "FONT %[^\n]", facename) != 1) {
                fprintf(stderr, "Error: bad 'FONT'\n");
                return 0;
            }
            pf->facename = strdup(facename);
            continue;
        }
        if (isprefix(buf, "COPYRIGHT ")) {	/* not required*/
            if (sscanf(buf, "COPYRIGHT \"%[^\"]", copyright) != 1) {
                fprintf(stderr, "Error: bad 'COPYRIGHT'\n");
                return 0;
            }
            pf->copyright = strdup(copyright);
            continue;
        }
        if (isprefix(buf, "DEFAULT_CHAR ")) {	/* not required*/
            if (sscanf(buf, "DEFAULT_CHAR %d", &pf->defaultchar) != 1) {
                fprintf(stderr, "Error: bad 'DEFAULT_CHAR'\n");
                return 0;
            }
        }
        if (isprefix(buf, "FONT_DESCENT ")) {
            if (sscanf(buf, "FONT_DESCENT %d", &pf->descent) != 1) {
                fprintf(stderr, "Error: bad 'FONT_DESCENT'\n");
                return 0;
            }
            continue;
        }
        if (isprefix(buf, "FONT_ASCENT ")) {
            if (sscanf(buf, "FONT_ASCENT %d", &pf->ascent) != 1) {
                fprintf(stderr, "Error: bad 'FONT_ASCENT'\n");
                return 0;
            }
            continue;
        }
        if (isprefix(buf, "FONTBOUNDINGBOX ")) {
            if (sscanf(buf, "FONTBOUNDINGBOX %d %d %d %d",
                       &pf->fbbw, &pf->fbbh, &pf->fbbx, &pf->fbby) != 4) {
                fprintf(stderr, "Error: bad 'FONTBOUNDINGBOX'\n");
                return 0;
            }
            continue;
        }
        if (isprefix(buf, "CHARS ")) {
            if (sscanf(buf, "CHARS %d", &nchars) != 1) {
                fprintf(stderr, "Error: bad 'CHARS'\n");
                return 0;
            }
            continue;
        }

        /*
         * Reading ENCODING is necessary to get firstchar/lastchar
         * which is needed to pre-calculate our offset and widths
         * array sizes.
         */
        if (isprefix(buf, "ENCODING ")) {
            if (sscanf(buf, "ENCODING %d", &encoding) != 1) {
                fprintf(stderr, "Error: bad 'ENCODING'\n");
                return 0;
            }
            if (encoding >= 0 &&
                encoding <= limit_char && 
                encoding >= start_char) {

                if (firstchar > encoding)
                    firstchar = encoding;
                if (lastchar < encoding)
                    lastchar = encoding;
            }
            continue;
        }
        if (strequal(buf, "ENDFONT"))
            break;
    }

    /* calc font height*/
    if (pf->ascent < 0 || pf->descent < 0 || firstchar < 0) {
        fprintf(stderr, "Error: Invalid BDF file, requires FONT_ASCENT/FONT_DESCENT/ENCODING\n");
        return 0;
    }
    pf->height = pf->ascent + pf->descent;

    /* calc default char*/
    if (pf->defaultchar < 0 || 
        pf->defaultchar < firstchar || 
        pf->defaultchar > limit_char )
        pf->defaultchar = firstchar;

    /* calc font size (offset/width entries)*/
    pf->firstchar = firstchar;
    pf->size = lastchar - firstchar + 1;
	
    /* use the font boundingbox to get initial maxwidth*/
    /*maxwidth = pf->fbbw - pf->fbbx;*/
    maxwidth = pf->fbbw;

    /* initially use font maxwidth * height for bits allocation*/
    pf->bits_size = nchars * BITMAP_WORDS(maxwidth) * pf->height;

    /* allocate bits, offset, and width arrays*/
    pf->bits = (bitmap_t *)malloc(pf->bits_size * sizeof(bitmap_t) + EXTRA);
    pf->offset = (unsigned long *)malloc(pf->size * sizeof(unsigned long));
    pf->offrot = (unsigned long *)malloc(pf->size * sizeof(unsigned long));
    pf->width = (unsigned char *)malloc(pf->size * sizeof(unsigned char));
	
    if (!pf->bits || !pf->offset || !pf->offrot || !pf->width) {
        fprintf(stderr, "Error: no memory for font load\n");
        return 0;
    }

    fprintf(stderr, "Header parsed\n");
    return 1;
}

/* read bdf font bitmaps, return 0 on error*/
int bdf_read_bitmaps(FILE *fp, struct font* pf)
{
    long ofs = 0;
    long ofr = 0;
    int maxwidth = 0;
    int i, k, encoding, width;
    int bbw, bbh, bbx, bby;
    int proportional = 0;
    int encodetable = 0;
    long l;
    char buf[256];

    /* reset file pointer*/
    fseek(fp, 0L, SEEK_SET);

    /* initially mark offsets as not used*/
    for (i=0; i<pf->size; ++i)
        pf->offset[i] = -1;

    for (;;) {
        if (!bdf_getline(fp, buf, sizeof(buf))) {
            fprintf(stderr, "Error: EOF on file\n");
            return 0;
        }
        if (isprefix(buf, "STARTCHAR")) {
            encoding = width = bbw = bbh = bbx = bby = -1;
            continue;
        }
        if (isprefix(buf, "ENCODING ")) {
            if (sscanf(buf, "ENCODING %d", &encoding) != 1) {
                fprintf(stderr, "Error: bad 'ENCODING'\n");
                return 0;
            }
            if (encoding < start_char || encoding > limit_char)
                encoding = -1;
            continue;
        }
        if (isprefix(buf, "DWIDTH ")) {
            if (sscanf(buf, "DWIDTH %d", &width) != 1) {
                fprintf(stderr, "Error: bad 'DWIDTH'\n");
                return 0;
            }
            /* use font boundingbox width if DWIDTH <= 0*/
            if (width <= 0)
                width = pf->fbbw - pf->fbbx;
            continue;
        }
        if (isprefix(buf, "BBX ")) {
            if (sscanf(buf, "BBX %d %d %d %d", &bbw, &bbh, &bbx, &bby) != 4) {
                fprintf(stderr, "Error: bad 'BBX'\n");
                return 0;
            }
            continue;
        }
        if (strequal(buf, "BITMAP")) {
            bitmap_t *ch_bitmap = pf->bits + ofs;
            int ch_words;

            if (encoding < 0)
                continue;

            /* set bits offset in encode map*/
            if (pf->offset[encoding-pf->firstchar] != (unsigned long)-1) {
                fprintf(stderr, "Error: duplicate encoding for character %d (0x%02x), ignoring duplicate\n",
                        encoding, encoding);
                continue;
            }
            pf->offset[encoding-pf->firstchar] = ofs;
            pf->offrot[encoding-pf->firstchar] = ofr;

            /* calc char width*/
            if (bbx < 0) {
                width -= bbx;
				/*if (width > maxwidth)
                                  width = maxwidth;*/
                bbx = 0;
            }
            if (width > maxwidth)
                maxwidth = width;
            pf->width[encoding-pf->firstchar] = width;

            /* clear bitmap*/
            memset(ch_bitmap, 0, BITMAP_BYTES(width) * pf->height);

            ch_words = BITMAP_WORDS(width);
#define BM(row,col)	(*(ch_bitmap + ((row)*ch_words) + (col)))
#define BITMAP_NIBBLES	(BITMAP_BITSPERIMAGE/4)

            /* read bitmaps*/
            for (i=0; ; ++i) {
                int hexnibbles;

                if (!bdf_getline(fp, buf, sizeof(buf))) {
                    fprintf(stderr, "Error: EOF reading BITMAP data\n");
                    return 0;
                }
                if (isprefix(buf, "ENDCHAR"))
                    break;

                hexnibbles = strlen(buf);
                for (k=0; k<ch_words; ++k) {
                    int ndx = k * BITMAP_NIBBLES;
                    int padnibbles = hexnibbles - ndx;
                    bitmap_t value;
					
                    if (padnibbles <= 0)
                        break;
                    if (padnibbles >= BITMAP_NIBBLES)
                        padnibbles = 0;

                    value = bdf_hexval((unsigned char *)buf,
                                       ndx, ndx+BITMAP_NIBBLES-1-padnibbles);
                    value <<= padnibbles * BITMAP_NIBBLES;

                    BM(pf->height - pf->descent - bby - bbh + i, k) |=
                        value >> bbx;
                    /* handle overflow into next image word*/
                    if (bbx) {
                        BM(pf->height - pf->descent - bby - bbh + i, k+1) =
                            value << (BITMAP_BITSPERIMAGE - bbx);
                    }
                }
            }

            ofs += BITMAP_WORDS(width) * pf->height;
            ofr += pf->width[encoding-pf->firstchar] * ((pf->height+7)/8);

            continue;
        }
        if (strequal(buf, "ENDFONT"))
            break;
    }

    /* set max width*/
    pf->maxwidth = maxwidth;

    /* change unused offset/width values to default char values*/
    for (i=0; i<pf->size; ++i) {
        int defchar = pf->defaultchar - pf->firstchar;

        if (pf->offset[i] == (unsigned long)-1) {
            pf->offset[i] = pf->offset[defchar];
            pf->offrot[i] = pf->offrot[defchar];
            pf->width[i] = pf->width[defchar];
        }
    }

    /* determine whether font doesn't require encode table*/
#ifdef ROTATE
    l = 0;
    for (i=0; i<pf->size; ++i) {
        if (pf->offrot[i] != l) {
            encodetable = 1;
            break;
        }
        l += pf->maxwidth * ((pf->height + 7) / 8);
    }
#else
    l = 0;
    for (i=0; i<pf->size; ++i) {
        if (pf->offset[i] != l) {
            encodetable = 1;
            break;
        }
        l += BITMAP_WORDS(pf->width[i]) * pf->height;
    }
#endif
    if (!encodetable) {
        free(pf->offset);
        pf->offset = NULL;
    }

    /* determine whether font is fixed-width*/
    for (i=0; i<pf->size; ++i) {
        if (pf->width[i] != maxwidth) {
            proportional = 1;
            break;
        }
    }
    if (!proportional) {
        free(pf->width);
        pf->width = NULL;
    }

    /* reallocate bits array to actual bits used*/
    if (ofs < pf->bits_size) {
        pf->bits = realloc(pf->bits, ofs * sizeof(bitmap_t));
        pf->bits_size = ofs;
    }
    else {
        if (ofs > pf->bits_size) {
            fprintf(stderr, "Warning: DWIDTH spec > max FONTBOUNDINGBOX\n");
            if (ofs > pf->bits_size+EXTRA) {
                fprintf(stderr, "Error: Not enough bits initially allocated\n");
                return 0;
            }
            pf->bits_size = ofs;
        }
    }

#ifdef ROTATE
    pf->bits_size = ofr; /* always update, rotated is smaller */
#endif

    return 1;
}

/* read the next non-comment line, returns buf or NULL if EOF*/
char *bdf_getline(FILE *fp, char *buf, int len)
{
    int c;
    char *b;

    for (;;) {
        b = buf;
        while ((c = getc(fp)) != EOF) {
            if (c == '\r')
                continue;
            if (c == '\n')
                break;
            if (b - buf >= (len - 1))
                break;
            *b++ = c;
        }
        *b = '\0';
        if (c == EOF && b == buf)
            return NULL;
        if (b != buf && !isprefix(buf, "COMMENT"))
            break;
    }
    return buf;
}

/* return hex value of portion of buffer*/
bitmap_t bdf_hexval(unsigned char *buf, int ndx1, int ndx2)
{
    bitmap_t val = 0;
    int i, c;

    for (i=ndx1; i<=ndx2; ++i) {
        c = buf[i];
        if (c >= '0' && c <= '9')
            c -= '0';
        else 
            if (c >= 'A' && c <= 'F')
                c = c - 'A' + 10;
            else 
                if (c >= 'a' && c <= 'f')
                    c = c - 'a' + 10;
                else 
                    c = 0;
        val = (val << 4) | c;
    }
    return val;
}

/*
 * Take an bitmap_t bitmap and convert to Rockbox format.
 * Used for converting font glyphs for the time being.
 * Can use for standard X11 and Win32 images as well.
 * See format description in lcd-recorder.c
 *
 * Doing it this way keeps fonts in standard formats,
 * as well as keeping Rockbox hw bitmap format.
 */
int rotleft(unsigned char *dst, bitmap_t *src, unsigned int width,
                    unsigned int height)
{
    unsigned int i,j;
    unsigned int src_words;        /* # words of input image*/
    unsigned int dst_mask;      /* bit mask for destination */
    bitmap_t src_mask;          /* bit mask for source */

    /* calc words of input image*/
    src_words = BITMAP_WORDS(width) * height;

    /* clear background*/
    memset(dst, 0, ((height + 7) / 8) * width);

    dst_mask = 1;

    for (i=0; i < src_words; i++) {

        /* calc src input bit*/
        src_mask = 1 << (sizeof (bitmap_t) * 8 - 1);
        
        /* for each input column...*/
        for(j=0; j < width; j++) {

            if (src_mask == 0) /* input word done? */
            {
                src_mask = 1 << (sizeof (bitmap_t) * 8 - 1);
                i++;    /* next input word */
            }

            /* if set in input, set in rotated output */
            if (src[i] & src_mask)
                dst[j] |= dst_mask;

            src_mask >>= 1;    /* next input bit */
        }

        dst_mask <<= 1;          /* next output bit (row) */
        if (dst_mask > (1 << 7)) /* output bit > 7? */
        {
            dst_mask = 1;
            dst += width;        /* next output byte row */
        }
    }
    return ((height + 7) / 8) * width; /* return result size in bytes */
}


/* generate C source from in-core font*/
int gen_c_source(struct font* pf, char *path)
{
    FILE *ofp;
    int i, ofr = 0;
    int did_defaultchar = 0;
    int did_syncmsg = 0;
    time_t t = time(0);
    bitmap_t *ofs = pf->bits;
    char buf[256];
    char obuf[256];
    char hdr1[] = {
        "/* Generated by convbdf on %s. */\n"
        "#include \"font.h\"\n"
        "#ifdef HAVE_LCD_BITMAP\n"
        "\n"
        "/* Font information:\n"
        "   name: %s\n"
        "   facename: %s\n"
        "   w x h: %dx%d\n"
        "   size: %d\n"
        "   ascent: %d\n"
        "   descent: %d\n"
        "   first char: %d (0x%02x)\n"
        "   last char: %d (0x%02x)\n"
        "   default char: %d (0x%02x)\n"
        "   proportional: %s\n"
        "   %s\n"
        "*/\n"
        "\n"
        "/* Font character bitmap data. */\n"
        "static const unsigned char _font_bits[] = {\n"
    };

    ofp = fopen(path, "w");
    if (!ofp) {
        fprintf(stderr, "Can't create %s\n", path);
        return 1;
    }

    strcpy(buf, ctime(&t));
    buf[strlen(buf)-1] = 0;

    fprintf(ofp, hdr1, buf, 
            pf->name,
            pf->facename? pf->facename: "",
            pf->maxwidth, pf->height,
            pf->size,
            pf->ascent, pf->descent,
            pf->firstchar, pf->firstchar,
            pf->firstchar+pf->size-1, pf->firstchar+pf->size-1,
            pf->defaultchar, pf->defaultchar,
            pf->width? "yes": "no",
            pf->copyright? pf->copyright: "");

    /* generate bitmaps*/
    for (i=0; i<pf->size; ++i) {
        int x;
        int bitcount = 0;
        int width = pf->width ? pf->width[i] : pf->maxwidth;
        int height = pf->height;
        bitmap_t *bits = pf->bits + (pf->offset? pf->offset[i]: (height * i));
        bitmap_t bitvalue;

        /*
         * Generate bitmap bits only if not this index isn't
         * the default character in encode map, or the default
         * character hasn't been generated yet.
         */
        if (pf->offset && 
            (pf->offset[i] == pf->offset[pf->defaultchar-pf->firstchar])) {
            if (did_defaultchar) {
                pf->offrot[i] = pf->offrot[pf->defaultchar-pf->firstchar];
                continue;
            }
            did_defaultchar = 1;
        }

        fprintf(ofp, "\n/* Character %d (0x%02x):\n   width %d",
                i+pf->firstchar, i+pf->firstchar, width);

        if (gen_map) {
            fprintf(ofp, "\n   +");
            for (x=0; x<width; ++x) fprintf(ofp, "-");
            fprintf(ofp, "+\n");

            x = 0;
            while (height > 0) {
                if (x == 0) fprintf(ofp, "   |");

                if (bitcount <= 0) {
                    bitcount = BITMAP_BITSPERIMAGE;
                    bitvalue = *bits++;
                }

                fprintf(ofp, BITMAP_TESTBIT(bitvalue)? "*": " ");

                bitvalue = BITMAP_SHIFTBIT(bitvalue);
                --bitcount;
                if (++x == width) {
                    fprintf(ofp, "|\n");
                    --height;
                    x = 0;
                    bitcount = 0;
                }
            }
            fprintf(ofp, "   +");
            for (x=0; x<width; ++x)
                fprintf(ofp, "-");
            fprintf(ofp, "+ */\n");
        }
        else
            fprintf(ofp, " */\n");

        bits = pf->bits + (pf->offset? pf->offset[i]: (pf->height * i));
#ifdef ROTATE /* pre-rotated into Rockbox bitmap format */
        {
          unsigned char bytemap[256];
          int y8, ix=0;
          
          int size = rotleft(bytemap, bits, width, pf->height);
          for (y8=0; y8<pf->height; y8+=8) /* column rows */
          {
            for (x=0; x<width; x++) {
                fprintf(ofp, "0x%02x, ", bytemap[ix]);
                ix++;
            }	
            fprintf(ofp, "\n");
          }

        /* update offrot since bits are now in sorted order */
        pf->offrot[i] = ofr;
        ofr += size;

        }
#else
        for (x=BITMAP_WORDS(width)*pf->height; x>0; --x) {
            fprintf(ofp, "0x%04x,\n", *bits);
            if (!did_syncmsg && *bits++ != *ofs++) {
                fprintf(stderr, "Warning: found encoding values in non-sorted order (not an error).\n");
                did_syncmsg = 1;
            }
        }	
#endif
    }
    fprintf(ofp, "};\n\n");

    if (pf->offset) {
        /* output offset table*/
        fprintf(ofp, "/* Character->glyph mapping. */\n"
                "static const unsigned short _sysfont_offset[] = {\n");

        for (i=0; i<pf->size; ++i)
            fprintf(ofp, "  %ld,\t/* (0x%02x) */\n", 
#ifdef ROTATE
                    pf->offrot[i], i+pf->firstchar);
#else
                    pf->offset[i], i+pf->firstchar);
#endif
        fprintf(ofp, "};\n\n");
    }

    /* output width table for proportional fonts*/
    if (pf->width) {
        fprintf(ofp, "/* Character width data. */\n"
                "static const unsigned char _sysfont_width[] = {\n");

        for (i=0; i<pf->size; ++i)
            fprintf(ofp, "  %d,\t/* (0x%02x) */\n", 
                    pf->width[i], i+pf->firstchar);
        fprintf(ofp, "};\n\n");
    }

    /* output struct font struct*/
    if (pf->offset)
        sprintf(obuf, "_sysfont_offset,");
    else
        sprintf(obuf, "0,  /* no encode table */");

    if (pf->width)
        sprintf(buf, "_sysfont_width,  /* width */");
    else
        sprintf(buf, "0,  /* fixed width */");

    fprintf(ofp, 	"/* Exported structure definition. */\n"
            "const struct font sysfont = {\n"
            "  %d,  /* maxwidth */\n"
            "  %d,  /* height */\n"
            "  %d,  /* ascent */\n"
            "  %d,  /* firstchar */\n"
            "  %d,  /* size */\n"
            "  _font_bits, /* bits */\n"
            "  %s  /* offset */\n"
            "  %s\n"
            "  %d,  /* defaultchar */\n"
            "  %d   /* bits_size */\n"
            "};\n"
            "#endif /* HAVE_LCD_BITMAP */\n",
            pf->maxwidth, pf->height,
            pf->ascent,
            pf->firstchar,
            pf->size,
            obuf,
            buf,
            pf->defaultchar,
            pf->bits_size);

    return 0;
}

static int writebyte(FILE *fp, unsigned char c)
{
    return putc(c, fp) != EOF;
}

static int writeshort(FILE *fp, unsigned short s)
{
    putc(s, fp);
    return putc(s>>8, fp) != EOF;
}

static int writelong(FILE *fp, unsigned long l)
{
    putc(l, fp);
    putc(l>>8, fp);
    putc(l>>16, fp);
    return putc(l>>24, fp) != EOF;
}

static int writestr(FILE *fp, char *str, int count)
{
    return fwrite(str, 1, count, fp) == count;
}

static int writestrpad(FILE *fp, char *str, int totlen)
{
    int ret;
	
    while (str && *str && totlen > 0) {
        if (*str) {
            ret = putc(*str++, fp);
            --totlen;
        }
    }
    while (--totlen >= 0)
        ret = putc(' ', fp);
    return ret;
}

/* generate .fnt format file from in-core font*/
int gen_fnt_file(struct font* pf, char *path)
{
    FILE *ofp;
    int i;
    int did_defaultchar = 0;
#ifdef ROTATE
    int ofr = 0;
#endif

    ofp = fopen(path, "wb");
    if (!ofp) {
        fprintf(stderr, "Can't create %s\n", path);
        return 1;
    }

    /* write magic and version #*/
    writestr(ofp, VERSION, 4);
#ifndef ROTATE
    /* internal font name*/
    writestrpad(ofp, pf->name, 64);

    /* copyright*/
    writestrpad(ofp, pf->copyright, 256);
#endif
    /* font info*/
    writeshort(ofp, pf->maxwidth);
    writeshort(ofp, pf->height);
    writeshort(ofp, pf->ascent);
    writeshort(ofp, 0);
    writelong(ofp, pf->firstchar);
    writelong(ofp, pf->defaultchar);
    writelong(ofp, pf->size);

    /* variable font data sizes*/
    writelong(ofp, pf->bits_size);		  /* # words of bitmap_t*/
    writelong(ofp, pf->offset? pf->size: 0);  /* # longs of offset*/
    writelong(ofp, pf->width? pf->size: 0);	  /* # bytes of width*/
    /* variable font data*/
#ifdef ROTATE
    for (i=0; i<pf->size; ++i)
    {
        bitmap_t* bits = pf->bits + (pf->offset? pf->offset[i]: (pf->height * i));
        int width = pf->width ? pf->width[i] : pf->maxwidth;
        int size;  
        unsigned char bytemap[256];
  
        if (pf->offset && 
            (pf->offset[i] == pf->offset[pf->defaultchar-pf->firstchar])) {
            if (did_defaultchar) {
                pf->offrot[i] = pf->offrot[pf->defaultchar-pf->firstchar];
                continue;
            }
            did_defaultchar = 1;
        }

        size = rotleft(bytemap, bits, width, pf->height);
        writestr(ofp, (char *)bytemap, size);

        /* update offrot since bits are now in sorted order */
        pf->offrot[i] = ofr;
        ofr += size;
    }

    if ( pf->bits_size < 0xFFDB )
    {
        /* bitmap offset is small enough, use unsigned short for offset */
        if (ftell(ofp) & 1)
            writebyte(ofp, 0);          /* pad to 16-bit boundary*/
    }
    else
    {
        /* bitmap offset is large then 64K, use unsigned long for offset */
        while (ftell(ofp) & 3)
            writebyte(ofp, 0);          /* pad to 32-bit boundary*/
    }

    if (pf->offset)
    {
        for (i=0; i<pf->size; ++i)
        {
            if ( pf->bits_size < 0xFFDB )
                writeshort(ofp, pf->offrot[i]);
            else
                writelong(ofp, pf->offrot[i]);
        }
    }

    if (pf->width)
        for (i=0; i<pf->size; ++i)
            writebyte(ofp, pf->width[i]);
#else
    for (i=0; i<pf->bits_size; ++i)
        writeshort(ofp, pf->bits[i]);
    if (ftell(ofp) & 2)
        writeshort(ofp, 0);		/* pad to 32-bit boundary*/

    if (pf->offset)
        for (i=0; i<pf->size; ++i)
            writelong(ofp, pf->offset[i]);

    if (pf->width)
        for (i=0; i<pf->size; ++i)
            writebyte(ofp, pf->width[i]);
#endif
    fclose(ofp);
    return 0;
}
