/****************************************************************************\
 Part of the XeTeX typesetting system
 copyright (c) 1994-2005 by SIL International
 written by Jonathan Kew

 This software is distributed under the terms of the Common Public License,
 version 1.0.
 For details, see <http://www.opensource.org/licenses/cpl1.0.php> or the file
 cpl1.0.txt included with the software.
\****************************************************************************/

/*
	xdv2pdf
	Convert xdv file from XeTeX to PDF format for viewing/printing

	usage: xdv2pdf [-d debug] [-m mag] [-p papersize] [-v] xdvfile [-o pdffile]

		-m	override magnification from xdv file
		-v	show progress messages (page counters)
		-d  set kpathsea_debug values
        -p	papersize [default: from Mac OS X printing system]
        
        known papersize values:
            a0-a10
			b0-b10
			c0-c10
			jb0-jb10
            letter
            legal
            ledger
            tabloid
		can append ":landscape"
		or use "x,y" where x and y are dimensions in "big points" or with units
        
		output file defaults to <xdvfile>.pdf

	also permissible to omit xdvfile but specify pdffile; then input is from stdin
*/

#define MAC_OS_X_VERSION_MIN_REQUIRED	1020
#define MAC_OS_X_VERSION_MAX_ALLOWED	1030

#include <ApplicationServices/ApplicationServices.h>
#include <Quicktime/Quicktime.h>
#include <string>
#include <vector>
#include <list>
#include <map>

#include <sys/stat.h>

#include "DVIops.h"

#define XDV_ID	5	// current id_byte value for .xdv files from xetex

#define AAT_FONT_FLAG	0xffff

#define DEFINE_GLOBALS	1
#include "xdv2pdf_globals.h"

paperSizeRec	gPaperSizes[] =
{
	{ "a0",		2383.937008,	3370.393701	},
	{ "a1",		1683.779528,	2383.937008	},
	{ "a2",		1190.551181,	1683.779528	},
	{ "a3",		841.8897638,	1190.551181	},
	{ "a4",		595.2755906,	841.8897638	},
	{ "a5",		419.5275591,	595.2755906	},
	{ "a6",		297.6377953,	419.5275591	},
	{ "a7",		209.7637795,	297.6377953	},
	{ "a8",		147.4015748,	209.7637795	},
	{ "a9",		104.8818898,	147.4015748	},
	{ "a10",	73.7007874,		104.8818898	},
	
	{ "b0",		2834.645669,	4008.188976	},
	{ "b1",		2004.094488,	2834.645669	},
	{ "b2",		1417.322835,	2004.094488	},
	{ "b3",		1000.629921,	1417.322835	},
	{ "b4",		708.6614173,	1000.629921	},
	{ "b5",		498.8976378,	708.6614173	},
	{ "b6",		354.3307087,	498.8976378	},
	{ "b7",		249.4488189,	354.3307087	},
	{ "b8",		175.7480315,	249.4488189	},
	{ "b9",		124.7244094,	175.7480315	},
	{ "b10",	87.87401575,	124.7244094	},
	
	{ "c0",		2599.370079,	3676.535433	},
	{ "c1",		1836.850394,	2599.370079	},
	{ "c2",		1298.267717,	1836.850394	},
	{ "c3",		918.4251969,	1298.267717	},
	{ "c4",		649.1338583,	918.4251969	},
	{ "c5",		459.2125984,	649.1338583	},
	{ "c6",		323.1496063,	459.2125984	},
	{ "c7",		229.6062992,	323.1496063	},
	{ "c8",		161.5748031,	229.6062992	},
	{ "c9",		113.3858268,	161.5748031	},
	{ "c10",	79.37007874,	113.3858268	},
	
	{ "jb0",	2919.685039,	4127.244094	},
	{ "jb1",	2063.622047,	2919.685039	},
	{ "jb2",	1459.84252,		2063.622047	},
	{ "jb3",	1031.811024,	1459.84252	},
	{ "jb4",	728.503937,		1031.811024	},
	{ "jb5",	515.9055118,	728.503937	},
	{ "jb6",	362.8346457,	515.9055118	},
	{ "jb7",	257.9527559,	362.8346457	},
	{ "jb8",	181.4173228,	257.9527559	},
	{ "jb9",	127.5590551,	181.4173228	},
	{ "jb10",	90.70866142,	127.5590551	},

	{ "letter",		612.0,	792.0	},
	{ "legal",		612.0,	1008.0	},
	{ "tabloid",	792.0,	1224.0	},
	{ "ledger",		1224.0,	792.0	},

	{ 0, 0, 0 }
};


struct nativeFont {
				nativeFont()
					: atsuStyle(0)
                    , cgFont(0)
                    , size(Long2Fix(10))
                    , isColored(false)
						{
						}
	ATSUStyle			atsuStyle;
	CGFontRef			cgFont;
	Fixed				size;
	ATSURGBAlphaColor	color;
	bool				isColored;
};

static std::map<UInt32,nativeFont>	sNativeFonts;

static ATSUTextLayout	sLayout;

static const UInt32	kUndefinedFont = 0x80000000;

static UInt32	sMag = 0;

static bool	sVerbose = false;

static std::map<ATSFontRef,CGFontRef>	sCGFonts;

static CGFontRef
getCGFontForATSFont(ATSFontRef fontRef)
{
	std::map<ATSFontRef,CGFontRef>::const_iterator	i = sCGFonts.find(fontRef);
	if (i != sCGFonts.end()) {
		return i->second;
	}
	CGFontRef	newFont;
	if (fontRef == 0) {
		ATSFontRef	tmpRef = ATSFontFindFromPostScriptName(CFSTR("Helvetica"), kATSOptionFlagsDefault);
		newFont = CGFontCreateWithPlatformFont(&tmpRef);
	}
	else
		newFont = CGFontCreateWithPlatformFont(&fontRef);
	sCGFonts[fontRef] = newFont;
	return newFont;
}


void
initAnnotBox()
{
	gAnnotBox.llx = gPageWd;
	gAnnotBox.lly = 0;
	gAnnotBox.urx = 0;
	gAnnotBox.ury = gPageHt;
}

void
flushAnnotBox()
{
	char	buf[200];
	sprintf(buf, "ABOX [%f %f %f %f]",
					kDvi2Scr * gAnnotBox.llx + 72.0,
					kDvi2Scr * (gPageHt - gAnnotBox.lly) - 72.0,
					kDvi2Scr * gAnnotBox.urx + 72.0,
					kDvi2Scr * (gPageHt - gAnnotBox.ury) - 72.0);
	doPDFspecial(buf);
	initAnnotBox();
}

static void
mergeBox(const box& b)
{
	if (b.llx < gAnnotBox.llx)
		gAnnotBox.llx = b.llx;
	if (b.lly > gAnnotBox.lly)
		gAnnotBox.lly = b.lly;	// NB positive is downwards!
	if (b.urx > gAnnotBox.urx)
		gAnnotBox.urx = b.urx;
	if (b.ury < gAnnotBox.ury)
		gAnnotBox.ury = b.ury;
}

long
readSigned(FILE* f, int k)
{
	long	s = (long)(signed char)getc(f);
	while (--k > 0) {
		s <<= 8;
		s += (getc(f) & 0xff);
	}
	return s;
}

unsigned long
readUnsigned(FILE* f, int k)
{
	unsigned long	u = getc(f);
	while (--k > 0) {
		u <<= 8;
		u += (getc(f) & 0xff);
	}
	return u;
}


void
ensurePageStarted()
{
	if (gPageStarted)
		return;
	
	CGContextSaveGState(gCtx);
    CGContextBeginPage(gCtx, &gMediaBox);

	static CGAffineTransform	sInitTextMatrix;
	if (gPageIndex == 1) {
		sInitTextMatrix = CGContextGetTextMatrix(gCtx);
		gUserColorSpace = 
			(&CGColorSpaceCreateWithName) != NULL
				? CGColorSpaceCreateWithName(kCGColorSpaceUserRGB)
				: CGColorSpaceCreateDeviceRGB();
	}
	else
		CGContextSetTextMatrix(gCtx, sInitTextMatrix);

	CGContextSetFillColorSpace(gCtx, gUserColorSpace);

	CGContextTranslateCTM(gCtx, 72.0, -72.0);

    gPageWd = kScr2Dvi * gMediaBox.size.width;
    gPageHt = kScr2Dvi * gMediaBox.size.height;
	
	cur_f = kUndefinedFont;
	
	gPageStarted = true;
}

#define MAX_BUFFERED_GLYPHS	1024
long		gGlyphCount = 0;
CGGlyph		gGlyphs[MAX_BUFFERED_GLYPHS];
CGSize		gAdvances[MAX_BUFFERED_GLYPHS];
CGPoint		gStartLoc;
CGPoint		gPrevLoc;

void
flushGlyphs()
{
	if (gGlyphCount > 0) {
		CGContextSetTextPosition(gCtx, gStartLoc.x, gStartLoc.y);
		gAdvances[gGlyphCount].width = 0;
		gAdvances[gGlyphCount].height = 0;
		if (gTextColor.override) {
			CGContextSaveGState(gCtx);
			CGContextSetFillColor(gCtx, &gTextColor.color.red);
		}
		CGContextShowGlyphsWithAdvances(gCtx, gGlyphs, gAdvances, gGlyphCount);
		if (gTextColor.override)
			CGContextRestoreGState(gCtx);
		gGlyphCount = 0;
	}
}

void
bufferGlyph(CGGlyph g)
{
	ensurePageStarted();

	CGPoint	curLoc;
	curLoc.x = kDvi2Scr * dvi.h;
	curLoc.y = kDvi2Scr * (gPageHt - dvi.v);
	if (gGlyphCount == 0)
		gStartLoc = curLoc;
	else {
		gAdvances[gGlyphCount-1].width = curLoc.x - gPrevLoc.x;
		gAdvances[gGlyphCount-1].height = curLoc.y - gPrevLoc.y;
	}
	gPrevLoc = curLoc;
	gGlyphs[gGlyphCount] = g;
	if (++gGlyphCount == MAX_BUFFERED_GLYPHS)
		flushGlyphs();
}

void
setChar(UInt32 c, bool adv)
{
	OSStatus	status;

	ensurePageStarted();

    if (f != cur_f) {
		flushGlyphs();
        CGContextSetFont(gCtx, gTeXFonts[f].cgFont);
        CGContextSetFontSize(gCtx, Fix2X(gTeXFonts[f].size) * gMagScale);
        cur_f = f;
	}

    CGGlyph	g;
	if (gTeXFonts[f].charMap != NULL) {
		if (c < gTeXFonts[f].charMap->size())
			g = (*gTeXFonts[f].charMap)[c];
		else
			g = c;
	}
	else
		// default: glyph ID = char + 1, works for the majority of the CM PS fonts
	    g = c + 1;
	    
	bufferGlyph(g);

	if (gTrackingBoxes) {
		if (gTeXFonts[f].hasMetrics) {
			box	b = {
				dvi.h,
				dvi.v + gTeXFonts[f].depths[c],
				dvi.h + gTeXFonts[f].widths[c],
				dvi.v - gTeXFonts[f].heights[c] };
			mergeBox(b);
		}
	}

	if (adv && gTeXFonts[f].hasMetrics)
		dvi.h += gTeXFonts[f].widths[c];
}

static void
doSetGlyph(UInt16 g, Fixed a)
{
	// NOTE that this is separate from the glyph-buffering done by setChar()
	// as \XeTeXglyph deals with native fonts, while setChar() and bufferGlyph()
	// are used to work with legacy TeX fonts
	
	// flush any setchar() glyphs, as we're going to change the font
	flushGlyphs();
	
	// always do this, as AAT fonts won't have set it yet
	// (might be redundant, but this is a rare operation anyway)
	CGContextSetFont(gCtx, sNativeFonts[f].cgFont);
	CGContextSetFontSize(gCtx, Fix2X(sNativeFonts[f].size) * gMagScale);
	cur_f = f;
	
	bool	resetColor = false;
    if (gTextColor.override) {
    	CGContextSaveGState(gCtx);
        CGContextSetFillColor(gCtx, &gTextColor.color.red);
		resetColor = true;
	}
	else if (sNativeFonts[f].isColored) {
    	CGContextSaveGState(gCtx);
        CGContextSetFillColor(gCtx, &sNativeFonts[f].color.red);
       	resetColor = true;
	}
    CGContextShowGlyphsAtPoint(gCtx, kDvi2Scr * dvi.h, kDvi2Scr * (gPageHt - dvi.v), &g, 1);
    if (resetColor)
    	CGContextRestoreGState(gCtx);

	if (gTrackingBoxes) {
		box	b = {
			dvi.h,
			dvi.v + (sNativeFonts[f].size / 3),
			dvi.h + a,
			dvi.v - (2 * sNativeFonts[f].size / 3) };
		mergeBox(b);
	}

	dvi.h += a;
}

void
doSetNative(FILE* xdv)
{
	OSStatus	status = noErr;
	Boolean	dir = readUnsigned(xdv, 1);
	UInt32	wid = readUnsigned(xdv, 4);
	UInt32	len = readUnsigned(xdv, 2);
    
	static UniChar*	text = 0;
    static UniCharCount	textBufLen = 0;
    if (len > textBufLen) {
        if (text != 0)
            delete[] text;
        textBufLen = (len & 0xFFFFFFF0) + 32;
        text = new UniChar[textBufLen];
    }

	UInt32	i;
	for (i = 0; i < len; ++i)
		text[i] = readUnsigned(xdv, 2);

	if (sNativeFonts[f].atsuStyle != 0) {
		status = ATSUClearLayoutCache(sLayout, 0);
		status = ATSUSetTextPointerLocation(sLayout, &text[0], 0, len, len);
	
		ATSUAttributeTag		tags[] = { kATSULineWidthTag, kATSULineJustificationFactorTag,
											kATSULineDirectionTag, kATSULineLayoutOptionsTag };
		ByteCount				valueSizes[] = { sizeof(Fixed), sizeof(Fract),
											sizeof(Boolean), sizeof(ATSLineLayoutOptions) };
		Fixed	scaledWid = X2Fix(kDvi2Scr * wid);
		Fract	just = len > 2 ? fract1 : 0;
		ATSLineLayoutOptions	options = kATSLineKeepSpacesOutOfMargin;
		ATSUAttributeValuePtr	values[] = { &scaledWid, &just, &dir, &options };
		status = ATSUSetLayoutControls(sLayout, sizeof(tags) / sizeof(ATSUAttributeTag), tags, valueSizes, values);
	
		ATSUStyle	tmpStyle;
		if (gTextColor.override) {
			ATSUAttributeTag		tag = kATSURGBAlphaColorTag;
			ByteCount				valueSize = sizeof(ATSURGBAlphaColor);
			ATSUAttributeValuePtr	valuePtr = &gTextColor.color;
			status = ATSUCreateAndCopyStyle(sNativeFonts[f].atsuStyle, &tmpStyle);
			status = ATSUSetAttributes(tmpStyle, 1, &tag, &valueSize, &valuePtr);
			status = ATSUSetRunStyle(sLayout, tmpStyle, kATSUFromTextBeginning, kATSUToTextEnd);
			cur_f = kUndefinedFont;
		}
		else if (f != cur_f) {
			status = ATSUSetRunStyle(sLayout, sNativeFonts[f].atsuStyle, kATSUFromTextBeginning, kATSUToTextEnd);
			cur_f = f;
		}
	
		CGContextTranslateCTM(gCtx, kDvi2Scr * dvi.h, kDvi2Scr * (gPageHt - dvi.v));
		status = ATSUDrawText(sLayout, 0, len, 0, 0);
		CGContextTranslateCTM(gCtx, -kDvi2Scr * dvi.h, -kDvi2Scr * (gPageHt - dvi.v));
	
		if (gTextColor.override)
			status = ATSUDisposeStyle(tmpStyle);

		if (gTrackingBoxes) {
			Rect	rect;
			status = ATSUMeasureTextImage(sLayout, 0, len, 0, 0, &rect);
			box	b = {
				dvi.h,
				dvi.v + kScr2Dvi * rect.bottom,
				dvi.h + wid,
				dvi.v + kScr2Dvi * rect.top };
			mergeBox(b);
		}
	}

	dvi.h += wid;
}

#define NATIVE_GLYPH_DATA_SIZE	(sizeof(UInt16) + sizeof(FixedPoint))
static void
doGlyphArray(FILE* xdv)
{
	static char*	glyphInfoBuf = 0;
	static int		maxGlyphs = 0;
	static CGSize*	advances = 0;

	flushGlyphs();

	Fixed	wid = readUnsigned(xdv, 4);
	int	glyphCount = readUnsigned(xdv, 2);
	
	if (glyphCount >= maxGlyphs) {
		maxGlyphs = glyphCount + 100;
		if (glyphInfoBuf != 0)
			delete[] glyphInfoBuf;
		glyphInfoBuf = new char[maxGlyphs * NATIVE_GLYPH_DATA_SIZE];
		if (advances != 0)
			delete[] advances;
		advances = new CGSize[maxGlyphs];
	}
	
	fread(glyphInfoBuf, NATIVE_GLYPH_DATA_SIZE, glyphCount, xdv);
	FixedPoint*	locations = (FixedPoint*)glyphInfoBuf;
	UInt16*		glyphs = (UInt16*)(locations + glyphCount);
	
	if (f != cur_f) {
		CGContextSetFont(gCtx, sNativeFonts[f].cgFont);
		CGContextSetFontSize(gCtx, Fix2X(sNativeFonts[f].size) * gMagScale);
		cur_f = f;
	}

	CGContextSetTextPosition(gCtx, kDvi2Scr * dvi.h + Fix2X(locations[0].x),
									kDvi2Scr * (gPageHt - dvi.v) - Fix2X(locations[0].y));

	for (int i = 0; i < glyphCount - 1; ++i) {
		advances[i].width = Fix2X(locations[i+1].x - locations[i].x);
		advances[i].height = Fix2X(locations[i].y - locations[i+1].y);
	}
	advances[glyphCount-1].width = 0.0;
	advances[glyphCount-1].height = 0.0;

	bool	resetColor = false;
    if (gTextColor.override) {
    	CGContextSaveGState(gCtx);
        CGContextSetFillColor(gCtx, &gTextColor.color.red);
		resetColor = true;
	}
	else if (sNativeFonts[f].isColored) {
    	CGContextSaveGState(gCtx);
        CGContextSetFillColor(gCtx, &sNativeFonts[f].color.red);
		resetColor = true;
	}

	CGContextShowGlyphsWithAdvances(gCtx, glyphs, advances, glyphCount);

	if (resetColor)
		CGContextRestoreGState(gCtx);

	if (gTrackingBoxes) {
		box	b = {
			dvi.h,
			dvi.v + (sNativeFonts[f].size / 3),
			dvi.h + wid,
			dvi.v - (2 * sNativeFonts[f].size / 3) };
		mergeBox(b);
	}

	dvi.h += wid;
}

/* code lifted almost verbatim from Apple sample "QTtoCG" */
typedef struct {
	size_t width;
	size_t height;
	size_t bitsPerComponent;
	size_t bitsPerPixel;
	size_t bytesPerRow;
	size_t size;
	CGImageAlphaInfo ai;
	CGColorSpaceRef cs;
	unsigned char *data;
	CMProfileRef prof;
} BitmapInfo;

void readBitmapInfo(GraphicsImportComponent gi, BitmapInfo *bi)
{
	ComponentResult result;
	ImageDescriptionHandle imageDescH = NULL;
	ImageDescription *desc;
	Handle profile = NULL;
	
	result = GraphicsImportGetImageDescription(gi, &imageDescH);
	if( noErr != result || imageDescH == NULL ) {
		fprintf(stderr, "Error while retrieving image description");
		exit(1);
	}
	
	desc = *imageDescH;
	
	bi->width = desc->width;
	bi->height = desc->height;
	bi->bitsPerComponent = 8;
	bi->bitsPerPixel = 32;
	bi->bytesPerRow = (bi->bitsPerPixel * bi->width + 7)/8;
	bi->ai = (desc->depth == 32) ? kCGImageAlphaFirst : kCGImageAlphaNoneSkipFirst;
	bi->size = bi->bytesPerRow * bi->height;
	bi->data = (unsigned char*)malloc(bi->size);
	
	bi->cs = NULL;
	bi->prof = NULL;
	GraphicsImportGetColorSyncProfile(gi, &profile);
	if( NULL != profile ) {
		CMError err;
		CMProfileLocation profLoc;
		Boolean bValid, bPreferredCMMNotFound;

		profLoc.locType = cmHandleBasedProfile;
		profLoc.u.handleLoc.h = profile;
		
		err = CMOpenProfile(&bi->prof, &profLoc);
		if( err != noErr ) {
			fprintf(stderr, "Cannot open profile");
			exit(1);
		}
		
		/* Not necessary to validate profile, but good for debugging */
		err = CMValidateProfile(bi->prof, &bValid, &bPreferredCMMNotFound);
		if( err != noErr ) {
			fprintf(stderr, "Cannot validate profile : Valid: %d, Preferred CMM not found : %d", bValid, 
				  bPreferredCMMNotFound);
			exit(1);
		}
		
		bi->cs = CGColorSpaceCreateWithPlatformColorSpace( bi->prof );

		if( bi->cs == NULL ) {
			fprintf(stderr, "Error creating cg colorspace from csync profile");
			exit(1);
		}
		DisposeHandle(profile);
	}	
	
	if( imageDescH != NULL)
		DisposeHandle((Handle)imageDescH);
}

void getBitmapData(GraphicsImportComponent gi, BitmapInfo *bi)
{
	GWorldPtr gWorld;
	QDErr err = noErr;
	Rect boundsRect = { 0, 0, bi->height, bi->width };
	ComponentResult result;
	
	if( bi->data == NULL ) {
		fprintf(stderr, "no bitmap buffer available");
		exit(1);
	}
	
	err = NewGWorldFromPtr( &gWorld, k32ARGBPixelFormat, &boundsRect, NULL, NULL, 0, 
							(char*)bi->data, bi->bytesPerRow );
	if (noErr != err) {
		fprintf(stderr, "error creating new gworld - %d", err);
		exit(1);
	}
	
	if( (result = GraphicsImportSetGWorld(gi, gWorld, NULL)) != noErr ) {
		fprintf(stderr, "error while setting gworld");
		exit(1);
	}
	
	if( (result = GraphicsImportDraw(gi)) != noErr ) {
		fprintf(stderr, "error while drawing image through qt");
		exit(1);
	}
	
	DisposeGWorld(gWorld);	
}
/* end of code from "QTtoCG" */

struct imageRec {
	CGImageRef	ref;
	CGRect		bounds;
};
std::map<std::string,imageRec>			sCGImages;
std::map<std::string,CGPDFDocumentRef>	sPdfDocs;

static void
doPicFile(FILE* xdv, bool isPDF)	// t[4][6] p[2] l[2] a[l]
{
    CGAffineTransform	t;
    t.a = Fix2X(readSigned(xdv, 4));
    t.b = Fix2X(readSigned(xdv, 4));
    t.c = Fix2X(readSigned(xdv, 4));
    t.d = Fix2X(readSigned(xdv, 4));
    t.tx = Fix2X(readSigned(xdv, 4));
    t.ty = Fix2X(readSigned(xdv, 4));
	if (sMag != 1000)
		t = CGAffineTransformConcat(t, CGAffineTransformMakeScale(gMagScale, gMagScale));

    int		p = readSigned(xdv, 2);
    int		l = readUnsigned(xdv, 2);
    unsigned char*	pathname = new unsigned char[l + 1];
	for (int i = 0; i < l; ++i)
		pathname[i] = readUnsigned(xdv, 1);
	pathname[l] = 0;

	CFURLRef	url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, pathname, l, false);
	if (url != NULL) {
		std::string	pathString((char*)pathname, l);	// key for our map<>s of already-used images

		// is it a \pdffile instance?
		CGPDFDocumentRef	document = NULL;
		CGImageRef			image = NULL;
		CGRect				bounds;
		if (isPDF) {
			std::map<std::string,CGPDFDocumentRef>::const_iterator	i = sPdfDocs.find(pathString);
			if (i != sPdfDocs.end())
				document = i->second;
			else {
				document = CGPDFDocumentCreateWithURL(url);
				sPdfDocs[pathString] = document;
			}
			if (document != NULL) {
				int	nPages = CGPDFDocumentGetNumberOfPages(document);
				if (p < 0)			p = nPages + 1 + p;
				if (p > nPages)		p = nPages;
				if (p < 1)			p = 1;
				bounds = CGPDFDocumentGetMediaBox(document, p);
			}
		}

		// otherwise use GraphicsImport
		else {
			std::map<std::string,imageRec>::const_iterator	i = sCGImages.find(pathString);
			if (i != sCGImages.end()) {
				image = i->second.ref;
				bounds = i->second.bounds;
			}
			else {
				FSRef	ref;
				if (CFURLGetFSRef(url, &ref)) {
					FSSpec	spec;
					if (FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, &spec, NULL) == noErr) {
						ComponentInstance	gi;
						OSErr	result = GetGraphicsImporterForFile(&spec, &gi);
						if (result == noErr) {
							BitmapInfo	bi;
							readBitmapInfo(gi, &bi);
							getBitmapData(gi, &bi);
			
							if (bi.cs == NULL)
								bi.cs = CGColorSpaceCreateDeviceRGB();
							CGDataProviderRef	provider = CGDataProviderCreateWithData(NULL, bi.data, bi.size, NULL);
							image = CGImageCreate(bi.width, bi.height, bi.bitsPerComponent, bi.bitsPerPixel,
													bi.bytesPerRow, bi.cs, bi.ai, provider, NULL, 0, kCGRenderingIntentDefault);
							CGColorSpaceRelease(bi.cs);
		
							ImageDescriptionHandle	desc = NULL;
							result = GraphicsImportGetImageDescription(gi, &desc);
							bounds.origin.x = 0;
							bounds.origin.y = 0;
							bounds.size.width = (*desc)->width * 72.0 / Fix2X((*desc)->hRes);
							bounds.size.height = (*desc)->height * 72.0 / Fix2X((*desc)->vRes);
							DisposeHandle((Handle)desc);
							result = CloseComponent(gi);
						}
						imageRec	ir = { image, bounds };
						sCGImages[pathString] = ir;
					}
				}
			}
		}

		CGContextSaveGState(gCtx);

		CGContextTranslateCTM(gCtx, kDvi2Scr * dvi.h, kDvi2Scr * (gPageHt - dvi.v));
		CGContextConcatCTM(gCtx, t);

		if (document != NULL) {
			bounds.origin.x = bounds.origin.y = 0.0;
			CGContextDrawPDFDocument(gCtx, bounds, document, p);
		}
		else if (image != NULL)
			CGContextDrawImage(gCtx, bounds, image);

		if (gTrackingBoxes) {
			// figure out the corners of the transformed and positioned image,
			// and remember the lower left and upper right of the result
			CGPoint	p[4];
			p[0].x = bounds.origin.x;
			p[0].y = bounds.origin.y;
			p[1].x = bounds.origin.x;
			p[1].y = bounds.origin.y + bounds.size.height;
			p[2].x = bounds.origin.x + bounds.size.width;
			p[2].y = bounds.origin.y + bounds.size.height;
			p[3].x = bounds.origin.x + bounds.size.width;
			p[3].y = bounds.origin.y;
			
			CGPoint	ll = { MAXFLOAT, MAXFLOAT };
			CGPoint	ur = { -MAXFLOAT, -MAXFLOAT };

			t = CGContextGetCTM(gCtx);
			// now t is the CTM, including positioning as well as transformations of the image

			for (int i = 0; i < 4; ++i) {
				p[i] = CGPointApplyAffineTransform(p[i], t);
				if (p[i].x < ll.x)
					ll.x = p[i].x;
				if (p[i].y < ll.y)
					ll.y = p[i].y;
				if (p[i].x > ur.x)
					ur.x = p[i].x;
				if (p[i].y > ur.y)
					ur.y = p[i].y;
			}
			
			// convert back to dvi space and add to the annotation area
			box	b = {
				(ll.x - 72.0) * kScr2Dvi,
				gPageHt - (ur.y + 72.0) * kScr2Dvi,
				(ur.x - 72.0) * kScr2Dvi,
				gPageHt - (ll.y + 72.0) * kScr2Dvi
			};
			mergeBox(b);
		}

		CGContextRestoreGState(gCtx);

		CFRelease(url);
	}
	
}

/* declarations of KPATHSEARCH functions we use for finding TFMs and OTFs */
extern "C" {
    UInt8* kpse_find_file(const UInt8* name, int type, int must_exist);
    UInt8* uppercasify(const UInt8* s);
};
extern	unsigned kpathsea_debug;

#include "xdv_kpse_formats.h"

static void
loadMetrics(struct texFont& font, UInt8* name, Fixed d, Fixed s)
{
	UInt8*	pathname = kpse_find_file(name, xdv_kpse_tfm_format, true);
    if (pathname) {
        FILE*	tfmFile = fopen((char*)pathname, "rb");
        if (tfmFile != 0) {
            enum { lf = 0, lh, bc, ec, nw, nh, nd, ni, nl, nk, ne, np };
            SInt16	directory[12];
            fread(&directory[0], 2, 12, tfmFile);
            fseek(tfmFile, directory[lh] * 4, SEEK_CUR);
            int	nChars = directory[ec] - directory[bc] + 1;
            double_t	factor = Fix2X(d) / 16.0;
            if (s != d)
                factor = factor * Fix2X(s) / Fix2X(d);
            if (nChars > 0) {
                struct s_charinfo {
                    UInt8	widthIndex;
                    UInt8	heightDepth;
                    UInt8	italicIndex;
                    UInt8	remainder;
                };
                s_charinfo*	charInfo = new s_charinfo[nChars];
                fread(&charInfo[0], 4, nChars, tfmFile);
                Fixed*	widths = new Fixed[directory[nw]];
                fread(&widths[0], 4, directory[nw], tfmFile);
                Fixed*	heights = new Fixed[directory[nh]];
                fread(&heights[0], 4, directory[nh], tfmFile);
                Fixed*	depths = new Fixed[directory[nd]];
                fread(&depths[0], 4, directory[nd], tfmFile);
                
                font.widths.reserve(directory[ec] + 1);
                font.heights.reserve(directory[ec] + 1);
                font.depths.reserve(directory[ec] + 1);
                font.charMap = new std::vector<UInt16>;
                font.charMap->reserve(directory[ec] + 1);
                for (int i = 0; i < directory[bc]; ++i) {
                    font.widths.push_back(0);
                    font.heights.push_back(0);
                    font.depths.push_back(0);
                    font.charMap->push_back(0);
                }
                int	g = 0;
                for (int i = 0; i < nChars; ++i) {
                	if (charInfo[i].widthIndex == 0) {
						font.widths.push_back(0);
						font.heights.push_back(0);
						font.depths.push_back(0);
						font.charMap->push_back(0);
					}
					else {
						double_t	wid = Fix2X(widths[charInfo[i].widthIndex]) * factor;
						font.widths.push_back(X2Fix(wid));

						int	heightIndex = (charInfo[i].heightDepth >> 4);
						double_t	ht = Fix2X(heights[heightIndex]) * factor;
						font.heights.push_back(X2Fix(ht));
						
						int	depthIndex = (charInfo[i].heightDepth & 0x0f);
						double_t	dp = Fix2X(depths[depthIndex]) * factor;
						font.depths.push_back(X2Fix(dp));

						font.charMap->push_back(++g);
					}
                }
                
                delete[] widths;
                delete[] heights;
                delete[] depths;
                delete[] charInfo;
            }
    
            font.hasMetrics = true;
            fclose(tfmFile);
        }
        free(pathname);
    }
}

typedef std::vector<std::string>	encoding;
std::map<std::string,encoding>		sEncodings;

class fontMapRec {
public:
				fontMapRec()
					: cgFont(0)
					, cmap(NULL)
					, loaded(false)
					{ }

	std::string	psName;
	std::string	encName;
	std::string	pfbName;

	CGFontRef			cgFont;
	std::vector<UInt16>*cmap;

	bool		loaded;
};

std::map<std::string,fontMapRec>	psFontsMap;

static void
clearFontMap()
{
	std::map<std::string,fontMapRec>::iterator	i;
	for (i = psFontsMap.begin(); i != psFontsMap.end(); ++i) {
		if (i->second.cmap != NULL)
			delete i->second.cmap;
	}
	psFontsMap.clear();
}

void
doPdfMapLine(const char* line, char mode)
{
	if (*line == '%')
		return;
	while (*line == ' ' || *line == '\t')
		++line;
	if (*line < ' ')
		return;

	if (mode == 0) {
		switch (*line) {
			case '+':
			case '-':
			case '=':
				mode = *line;
				++line;
				while (*line == ' ' || *line == '\t')
					++line;
				if (*line < ' ')
					return;
				break;
			default:
				clearFontMap();
				mode = '+';
				break;
		}
	}

	const char*	b = line;
	const char*	e = b;
	while (*e > ' ')
		++e;
	std::string	tfmName(b, e - b);
	
	if (mode == '-') {
		psFontsMap.erase(tfmName);
		return;
			// erase existing entry
	}
	
	if ((mode == '+') && (psFontsMap.find(tfmName) != psFontsMap.end()))
		return;
			// don't add if entry already exists
	
	while (*e && *e <= ' ')
		++e;
	b = e;
	while (*e > ' ')
		++e;
	if (e > b) {
		fontMapRec	fr;
		fr.psName.assign(b, e - b);
		while (*e && *e <= ' ')
			++e;
		if (*e == '"') {	// skip quoted string; we don't do oblique, stretch, etc. (yet)
			++e;
			while (*e && *e != '"')
				++e;
			if (*e == '"')
				++e;
			while (*e && *e <= ' ')
				++e;
		}
		while (*e == '<') {
			++e;
			b = e;
			while (*e > ' ')
				++e;
			if (strncmp(e - 4, ".enc", 4) == 0) {
				fr.encName.assign(b, e - b);
			}
			else if (strncmp(e - 4, ".pfb", 4) == 0) {
				fr.pfbName.assign(b, e - b);
			}
/*
			else if (strncmp(e - 4, ".pfa", 4) == 0) {
				fr.otfName.assign(b, e - b - 4);
			}
*/
			while (*e && *e <= ' ')
				++e;
		}
		psFontsMap[tfmName] = fr;
	}
}

static bool	sMapFileLoaded = false;

void
doPdfMapFile(const char* fileName)
{
	char	mode = 0;

	while (*fileName == ' ' || *fileName == '\t')
		++fileName;
	if (*fileName < ' ')
		return;

	switch (*fileName) {
		case '+':
		case '-':
		case '=':
			mode = *fileName;
			++fileName;
			break;
		default:
			clearFontMap();
			mode = '+';
			break;
	}
	while (*fileName == ' ' || *fileName == '\t')
		++fileName;
	if (*fileName < ' ')
		return;

	bool	loaded = false;
	UInt8*	pathname = kpse_find_file((UInt8*)fileName, xdv_kpse_dvips_config_format, true);
	if (pathname) {
		FILE*	f = fopen((char*)pathname, "r");
		if (f != NULL) {
			char	line[1000];
			while (!feof(f)) {
				if (fgets(line, 999, f) == 0)
					break;
				doPdfMapLine(line, mode);

			}
			loaded = true;
			fclose(f);
			if (sVerbose)
				fprintf(stderr, "\n{fontmap: %s} ", pathname);
		}
		free(pathname);
	}
	if (!loaded)
		fprintf(stderr, "\n*** fontmap %s not found; texmf.cnf may be broken\n", fileName);
	else
		sMapFileLoaded = true;
}

// return a pointer to the encoding vector with the given name, loading it if necessary
// we don't really "parse" the .enc file
static const encoding*
getEncoding(const std::string& name)
{
	std::map<std::string,encoding>::iterator	i = sEncodings.find(name);
	if (i == sEncodings.end()) {
		encoding	enc;
		UInt8*	pathname = kpse_find_file((UInt8*)(name.c_str()), xdv_kpse_tex_ps_header_format, true);
		if (pathname != NULL) {
			FILE*	f = fopen((char*)pathname, "r");
			if (f != NULL) {
				int	c;
				bool	inVector = false;
				std::string	str;
				while ((c = getc(f)) != EOF) {
				got_ch:
					if (c == '%') {	// comment: skip rest of line
						while ((c = getc(f)) != EOF && c != '\r' && c != '\n')
							;
						goto got_ch;
					}
					if (c == '[') {
						inVector = true;
					}
					else if (c == '/' || c <= ' ' || c == ']' || c == EOF) {
						if (inVector && str.length() > 0)
							enc.push_back(str);
						str.clear();
					}
					else if (inVector && c != EOF)
						str.append(1, (char)c);
				}
				if (sVerbose)
					fprintf(stderr, "\n{encoding: %s} ", pathname);
				fclose(f);
			}
			free(pathname);
		}
		sEncodings[name] = enc;
		return &(sEncodings[name]);
	}

	return &(i->second);
}

static ATSFontRef
activateFromPath(const char* pathName)
{
	FSRef		fontFileRef;
	FSSpec		fontFileSpec;
	OSStatus	status = FSPathMakeRef((UInt8*)pathName, &fontFileRef, 0);
	if (status == noErr)
		status = FSGetCatalogInfo(&fontFileRef, 0, 0, 0, &fontFileSpec, 0);
	if (status == noErr) {
		ATSFontContainerRef containerRef;
		status = ATSFontActivateFromFileSpecification(&fontFileSpec, kATSFontContextLocal,
						kATSFontFormatUnspecified, 0, kATSOptionFlagsDefault, &containerRef);
		ATSFontRef	fontRef;
		ItemCount	count;
		status = ATSFontFindFromContainer(containerRef, 0, 1, &fontRef, &count);
		if ((status == noErr) && (count == 1))
			return fontRef;
		// failed, or container contained multiple fonts(!) -- confused
		ATSFontDeactivate(containerRef, 0, kATSOptionFlagsDefault);
	}
	return kATSUInvalidFontID;
}

struct postTable {
	Fixed	format;
	Fixed	italicAngle;
	SInt16	underlinePosition;
	SInt16	underlineThickness;
	UInt16	isFixedPitch;
	UInt16	reserved;
	UInt32	minMemType42;
	UInt32	maxMemType42;
	UInt32	minMemType1;
	UInt32	maxMemType1;
};

#include "appleGlyphNames.c"

#define	sfntCacheDir	"/Library/Caches/Type1-sfnt-fonts/"
#define	sfntWrapCommand	"T1Wrap"
#define sfntWrapSuffix	"-sfnt.otf"

static ATSFontRef
activatePFB(const char* pfbName)
{
	ATSFontRef fontRef = kATSUInvalidFontID;
	OSStatus	status = noErr;

	static int firstTime = 1;
	if (firstTime) {
		firstTime = 0;
		status = mkdir(sfntCacheDir, S_IRWXU | S_IRWXG | S_IRWXO);
		if (status != 0) {
			if (errno == EEXIST) {
				// clean up possible residue from past failed conversions
				system("/usr/bin/find " sfntCacheDir " -maxdepth 1 -empty -type f -delete");
				status = 0;
			}
			else
				fprintf(stderr, "*** failed to create sfnt cache directory %s (errno = %d), cannot activate .pfb fonts\n",
						sfntCacheDir, errno);
		}
	}
	
	char*	sfntName = new char[strlen(sfntCacheDir) + strlen(pfbName) + strlen(sfntWrapSuffix) + 1];
	strcpy(sfntName, sfntCacheDir);
	strcat(sfntName, pfbName);
	strcat(sfntName, sfntWrapSuffix);

	FSRef	ref;
	status = FSPathMakeRef((const UInt8*)sfntName, &ref, NULL);
	if (status == fnfErr) {
		char*	pathName = (char*)kpse_find_file((UInt8*)pfbName, xdv_kpse_pfb_format, true);
		if (pathName != NULL) {
			char* cmd = new char[strlen(sfntWrapCommand) + strlen(pathName) + strlen(sfntName) + 6];
			strcpy(cmd, sfntWrapCommand " ");
			strcat(cmd, pathName);
			strcat(cmd, " > ");
			strcat(cmd, sfntName);
			status = system(cmd);
			delete[] cmd;
			free(pathName);
		}
	}
	
	if (status == noErr)
		fontRef = activateFromPath(sfntName);

	delete[] sfntName;

	if (fontRef == kATSUInvalidFontID) {
		// try removing extension (.pfb) and looking for an .otf font...
		char*	baseName = new char[strlen(pfbName) + 1];
		strcpy(baseName, pfbName);
		char*	dot = strrchr(baseName, '.');
		if (dot != NULL)
			*dot = 0;
		char*	pathName = (char*)kpse_find_file((UInt8*)baseName, xdv_kpse_otf_format, true);
		delete[] baseName;
		if (pathName != NULL) {
			fontRef = activateFromPath(pathName);
			free(pathName);
		}
	}
	
	if (fontRef == kATSUInvalidFontID)
		fprintf(stderr, "\n*** font activation failed (status=%d): %s\n", status, pfbName);

	return fontRef;
}

static std::vector<UInt16>*
readMacRomanCmap(ATSFontRef fontRef)
{
	std::vector<UInt16>*	cmap = NULL;
	ByteCount	size;
	OSStatus	status = ATSFontGetTable(fontRef, 'cmap', 0, 0, 0, &size);
	if (status == noErr) {
		UInt8*	buffer = new UInt8[size];
		ATSFontGetTable(fontRef, 'cmap', 0, size, buffer, &size);

		struct cmapHeader {
			UInt16	version;
			UInt16	numSubtables;
		};
		struct subtableHeader {
			UInt16	platform;
			UInt16	encoding;
			UInt32	offset;
		};
		struct format0 {
			UInt16	format;
			UInt16	length;
			UInt16	language;
			UInt8	glyphIndex[256];
		};
		struct format6 {
			UInt16	format;
			UInt16	length;
			UInt16	language;
			UInt16	firstCode;
			UInt16	entryCount;
			UInt16	glyphIndex[1];
		};
		
		struct cmapHeader*	h = (struct cmapHeader*)buffer;
		struct subtableHeader*	sh = (struct subtableHeader*)(buffer + sizeof(struct cmapHeader));
		int	subtable = 0;
		cmap = new std::vector<UInt16>;
		cmap->reserve(256);
		while (subtable < h->numSubtables) {
			if ((sh->platform == 1) && (sh->encoding == 0)) {
				struct format0*	f0 = (struct format0*)(buffer + sh->offset);
				if (f0->format == 0) {
					for (int ch = 0; ch < 256; ++ch) {
						cmap->push_back(f0->glyphIndex[ch]);
					}
				}
				else if (f0->format == 6) {
					struct format6*	f6 = (struct format6*)f0;
					for (int ch = 0; ch < 256; ++ch) {
						if ((ch < f6->firstCode) || (ch >= f6->firstCode + f6->entryCount))
							cmap->push_back(0);
						else
							cmap->push_back(f6->glyphIndex[ch - f6->firstCode]);
					}
				}
				else {
					// unsupported cmap subtable format
					fprintf(stderr, "\n*** unsupported 'cmap' subtable format (%d)\n", f0->format);
				}
				break;
			}
			++subtable;
			++sh;
		}
		
		delete[] buffer;
	}
	
	return cmap;
}

static const fontMapRec*
getFontRec(const std::string& name)
{
	if (!sMapFileLoaded)
		doPdfMapFile("psfonts.map");

	std::map<std::string,fontMapRec>::iterator	i = psFontsMap.find(name);
	if (i == psFontsMap.end())
		return NULL;
	if (i->second.loaded)
		return &(i->second);

	fontMapRec	fr = i->second;

	ATSFontRef	fontRef = kATSUInvalidFontID;
	// if a filename is known, try to find and activate it

	if (fr.pfbName.length() > 0)
		fontRef = activatePFB(fr.pfbName.c_str());

	// if no downloadable file, see if it's installed in the OS
	if (fontRef == kATSUInvalidFontID)
	    fontRef = ATSFontFindFromPostScriptName(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, fr.psName.c_str(),
    	                                        CFStringGetSystemEncoding(), kCFAllocatorNull), kATSOptionFlagsDefault);

	if (fontRef == kATSUInvalidFontID)
		fprintf(stderr, "\n*** font %s (%s: file '%s') not found\n", name.c_str(), fr.psName.c_str(), fr.pfbName.c_str());

	if (fontRef != kATSUInvalidFontID) {
		// if re-encoding was called for, load the encoding vector and build a new cmap
		if (fr.encName.length() > 0) {
			const encoding* enc = getEncoding(fr.encName);
			if (enc != 0) {
				ByteCount	size;
				OSStatus	status = ATSFontGetTable(fontRef, 'post', 0, 0, 0, &size);
				if (status == noErr) {
					UInt8*	buffer = new UInt8[size];
					ATSFontGetTable(fontRef, 'post', 0, size, buffer, &size);
					postTable*	p = (postTable*)&buffer[0];
					std::map<std::string,UInt16>	name2gid;
					UInt16	g = 0;
					switch (p->format) {
						case 0x00010000:
							{
								char*	cp;
								while ((cp = appleGlyphNames[g]) != 0) {
									name2gid[cp] = g;
									++g;
								}
							}
							break;
						
						case 0x00020000:
							{
								UInt16*	n = (UInt16*)(p + 1);
								UInt16	numGlyphs = *n++;
								UInt8*	ps = (UInt8*)(n + numGlyphs);
								std::vector<std::string>	newNames;
								while (ps < buffer + size) {
									newNames.push_back(std::string((char*)ps + 1, *ps));
									ps += *ps + 1;
								}
								for (g = 0; g < numGlyphs; ++g) {
									if (*n < 258)
										name2gid[appleGlyphNames[*n]] = g;
									else
										name2gid[newNames[*n - 258]] = g;
									++n;
								}
							}
							break;
						
						case 0x00028000:
							fprintf(stderr, "\n*** format 2.5 'post' table not supported\n");
							break;
						
						case 0x00030000:
							// TODO: see if it's a CFF OpenType font, and if so, get the glyph names from the CFF data
							fprintf(stderr, "\n*** format 3 'post' table; cannot reencode font %s\n", name.c_str());
							break;
						
						case 0x00040000:
							fprintf(stderr, "\n*** format 4 'post' table not supported\n");
							break;
						
						default:
							fprintf(stderr, "\n*** unknown 'post' table format %08x\n");
							break;
					}
					if (fr.cmap != NULL)
						delete fr.cmap;
					fr.cmap = new std::vector<UInt16>;
					for (encoding::const_iterator i = enc->begin(); i != enc->end(); ++i) {
						std::map<std::string,UInt16>::const_iterator	g = name2gid.find(*i);
						if (g == name2gid.end())
							fr.cmap->push_back(0);
						else
							fr.cmap->push_back(g->second);
					}
				}
				else {
					fprintf(stderr, "\n*** no 'post' table found, unable to re-encode font %s\n", name.c_str());
				}
			}
		}
		else {
			// read the MacOS/Roman cmap, if available
			std::vector<UInt16>*	cmap = readMacRomanCmap(fontRef);
			if (fr.cmap != NULL)
				delete fr.cmap;
			fr.cmap = cmap;
		}
	}
	
	if (fontRef == kATSUInvalidFontID) {
		fprintf(stderr, "\n*** font %s (%s) not found, will substitute Helvetica glyphs\n", name.c_str(), fr.pfbName.c_str());
		fontRef = ATSFontFindFromPostScriptName(CFSTR("Helvetica"), kATSOptionFlagsDefault);
		if (fr.cmap != NULL)
			delete fr.cmap;
		fr.cmap = readMacRomanCmap(fontRef);
	}

	fr.cgFont = getCGFontForATSFont(fontRef);
	fr.loaded = true;
	
	psFontsMap[name] = fr;
	return &(psFontsMap[name]);
}

static void
doFontDef(FILE* xdv, int k)
{
    OSStatus	status;

    UInt32	f = readUnsigned(xdv, k);	// font number we're defining
    UInt32	c = readUnsigned(xdv, 4);	// TFM checksum
    Fixed	s = readUnsigned(xdv, 4);	// at size
    Fixed	d = readUnsigned(xdv, 4);	// design size
    
    UInt16	alen = readUnsigned(xdv, 1);	// area length
    UInt16	nlen = readUnsigned(xdv, 1);	// name length

    UInt8*	name = new UInt8[alen + nlen + 2];
    if (alen > 0) {
        fread(name, 1, alen, xdv);
        name[alen] = '/';
        fread(name + alen + 1, 1, nlen, xdv);
        nlen += alen + 1;
    }
    else
        fread(name, 1, nlen, xdv);
    name[nlen] = 0;

    texFont	font;
    loadMetrics(font, name, d, s);

	if (alen > 0)
		name = name + alen + 1;	// point past the area to get the name by itself for the remaining operations

    ATSFontRef	fontRef;

	// look for a map entry for this font name
	std::string	nameStr((char*)name);
	const fontMapRec*	fr = getFontRec(nameStr);
	if (fr != NULL) {
		font.cgFont = fr->cgFont;
		if (fr->cmap->size() > 0) {
			if (font.charMap != NULL)
				delete font.charMap;
			font.charMap = fr->cmap;	// replacing map that was synthesized from the tfm coverage
		}
	}
	else {
		/* try to find the font without the benefit of psfonts.map...
			first try the name as a pfb or otf filename
			and then as the PS name of an installed font
		*/
		
		// ****** FIXME ****** this needs re-working to read the 'cmap' properly
		
		fontRef = ATSFontFindFromPostScriptName(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, (char*)name,
												CFStringGetSystemEncoding(), kCFAllocatorNull), kATSOptionFlagsDefault);
		UInt8*	ucname = 0;
		if (fontRef == kATSUInvalidFontID) {
			ucname = uppercasify(name);
			fontRef = ATSFontFindFromPostScriptName(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, (char*)ucname,
												CFStringGetSystemEncoding(), kCFAllocatorNull), kATSOptionFlagsDefault);
		}
		
		if (ucname != 0)
			free(ucname);

		if (fontRef == kATSUInvalidFontID) {
			fprintf(stderr, "\n*** font %s not found in psfonts.map or host system; will substitute Helvetica glyphs\n", (char*)name);
			fontRef = ATSFontFindFromPostScriptName(CFSTR("Helvetica"), kATSOptionFlagsDefault);
			if (font.charMap != NULL)
				delete font.charMap;
			font.charMap = readMacRomanCmap(fontRef);
		}
		
		font.cgFont = getCGFontForATSFont(fontRef);

		delete[] name;
	}

    font.size = s;
    gTeXFonts.insert(std::pair<const UInt32,texFont>(f, font));
}

inline bool operator==(const ATSURGBAlphaColor& a, const ATSURGBAlphaColor& b)
{
	return (a.red == b.red
		&& a.green == b.green
		&& a.blue == b.blue
		&& a.alpha == b.alpha);
}

static void
doNativeFontDef(FILE* xdv)
{
	static ATSURGBAlphaColor	sRed = { 1.0, 0.0, 0.0, 1.0 };

    UInt32	f = readUnsigned(xdv, 4);	// font ID
    Fixed	s = readUnsigned(xdv, 4);	// size
    if (sMag != 1000)
    	s = X2Fix(gMagScale * Fix2X(s));
	UInt16	technologyFlag = readUnsigned(xdv, 2);
	if (technologyFlag == AAT_FONT_FLAG) {
		// AAT font
		ATSUStyle	style;
		OSStatus	status = ATSUCreateStyle(&style);
		int		n = readUnsigned(xdv, 2);	// number of features
		if (n > 0) {
			UInt16*	types = new UInt16[n];
			UInt16*	selectors = new UInt16[n];
			fread(types, 2, n, xdv);
			fread(selectors, 2, n, xdv);
			status = ATSUSetFontFeatures(style, n, types, selectors);
			delete[] selectors;
			delete[] types;
		}
		n = readUnsigned(xdv, 2); // number of variations
		if (n > 0) {
			UInt32*	axes = new UInt32[n];
			SInt32*	values = new SInt32[n];
			fread(axes, 4, n , xdv);
			fread(values, 4, n, xdv);
			status = ATSUSetVariations(style, n, axes, values);
			delete[] values;
			delete[] axes;
		}
		ATSURGBAlphaColor	rgba;
		fread(&rgba, 1, sizeof(ATSURGBAlphaColor), xdv);
		
		n = readUnsigned(xdv, 1);	// vertical? flag
		if (n) {
			ATSUAttributeTag			t = kATSUVerticalCharacterTag;
			ATSUVerticalCharacterType	vert = kATSUStronglyVertical;
			ByteCount					vs = sizeof(ATSUVerticalCharacterType);
			ATSUAttributeValuePtr		v = &vert;
			status = ATSUSetAttributes(style, 1, &t, &vs, &v);
		}
	
		n = readUnsigned(xdv, 2);	// name length
		char*	name = new char[n+1];
		fread(name, 1, n, xdv);
		name[n] = 0;
	
		ATSUFontID	fontID;
		status = ATSUFindFontFromName(name, n, kFontPostscriptName,
			kFontNoPlatformCode, kFontNoScriptCode, kFontNoLanguageCode, &fontID);
	
		delete[] name;
		
		int numTags = 4;
		ATSUAttributeTag		tag[4] = { kATSUHangingInhibitFactorTag, kATSUSizeTag, kATSURGBAlphaColorTag, kATSUFontTag };
		ByteCount				valueSize[4] = { sizeof(Fract), sizeof(Fixed), sizeof(ATSURGBAlphaColor), sizeof(ATSUFontID) };
		ATSUAttributeValuePtr	value[4];
		Fract					inhibit = fract1;
		value[0] = &inhibit;
		value[1] = &s;
		value[2] = &rgba;
		value[3] = &fontID;
		if (fontID == kATSUInvalidFontID) {
			value[2] = &sRed;
			numTags--;
		}
		status = ATSUSetAttributes(style, numTags, &tag[0], &valueSize[0], &value[0]);
	
		nativeFont	fontRec;
		fontRec.atsuStyle = style;
		fontRec.cgFont = getCGFontForATSFont(FMGetATSFontRefFromFont(fontID));
		fontRec.size = s;
		fontRec.color = rgba;
		fontRec.isColored = !(rgba == kBlackColor);
		sNativeFonts.insert(std::pair<const UInt32,nativeFont>(f, fontRec));
	}
	else {
		// OT font
		Fixed	color[4];
		fread(&color[0], 4, sizeof(Fixed), xdv);
		ATSURGBAlphaColor	rgba;
		rgba.red = Fix2X(color[0]);
		rgba.green = Fix2X(color[1]);
		rgba.blue = Fix2X(color[2]);
		rgba.alpha = Fix2X(color[3]);

		int	n = readUnsigned(xdv, 2);	// name length
		char*	name = new char[n+1];
		fread(name, 1, n, xdv);
		name[n] = 0;

		ATSUFontID	fontID;
		OSStatus	status = ATSUFindFontFromName(name, n, kFontPostscriptName,
			kFontNoPlatformCode, kFontNoScriptCode, kFontNoLanguageCode, &fontID);
		
		if (fontID == kATSUInvalidFontID)
			rgba = sRed;
		
		nativeFont	fontRec;
		fontRec.cgFont = getCGFontForATSFont(FMGetATSFontRefFromFont(fontID));
		fontRec.size = s;
		fontRec.color = rgba;
		fontRec.isColored = !(rgba == kBlackColor);
		sNativeFonts.insert(std::pair<const UInt32,nativeFont>(f, fontRec));
	}
}

static void
processPage(FILE* xdv)
{
	/* enter here having just read BOP from xdv file */
	++gPageIndex;

	OSStatus		status;
	int				i;

	std::list<dviVars>	stack;
	gDviLevel = 0;

    int	j = 0;
    for (i = 0; i < 10; ++i) {	// read counts
		gCounts[i] = readSigned(xdv, 4);
        if (gCounts[i] != 0)
            j = i;
    }
	if (sVerbose) {
		fprintf(stderr, "[");
		for (i = 0; i < j; ++i)
			fprintf(stderr, "%d.", gCounts[i]);
		fprintf(stderr, "%d", gCounts[j]);
	}

    (void)readUnsigned(xdv, 4);	// skip prev-BOP pointer
	
	cur_f = kUndefinedFont;
	dvi.h = dvi.v = 0;
	dvi.w = dvi.x = dvi.y = dvi.z = 0;
	f = 0;
	
	unsigned int	cmd = DVI_BOP;
	while (cmd != DVI_EOP) {
		UInt32	u;
		SInt32	s;
		SInt32	ht, wd;
		int		k = 1;

		cmd = readUnsigned(xdv, 1);
		switch (cmd) {
			default:
				if (cmd < DVI_SET1)
					setChar(cmd, true);
				else if (cmd >= DVI_FNTNUM0 && cmd <= DVI_FNTNUM0 + 63)
					f = cmd - DVI_FNTNUM0;
				else
					goto ABORT_PAGE;
				break;
			
			case DVI_EOP:
				flushGlyphs();
				break;
			
			case DVI_NOP:
				break;
			
			case DVI_FNT4:
				++k;
			case DVI_FNT3:
				++k;
			case DVI_FNT2:
				++k;
			case DVI_FNT1:
				f = readUnsigned(xdv, k);	// that's it: just set |f|
				break;

			case DVI_XXX4:
				++k;
			case DVI_XXX3:
				++k;
			case DVI_XXX2:
				++k;
			case DVI_XXX1:
				flushGlyphs();
                u = readUnsigned(xdv, k);
                if (u > 0) {
                    char* special = new char[u+1];
                    fread(special, 1, u, xdv);
                    special[u] = '\0';
                    
                    doSpecial(special);
                    
                    delete[] special;
                }
				break;
			
			case DVI_SETRULE:
			case DVI_PUTRULE:
				ensurePageStarted();
				ht = readSigned(xdv, 4);
				wd = readSigned(xdv, 4);
				if (ht > 0 && wd > 0) {
					CGRect	r = CGRectMake(kDvi2Scr * dvi.h, kDvi2Scr * (gPageHt - dvi.v), kDvi2Scr * wd, kDvi2Scr * ht);
                    if (gRuleColor.override) {
                    	CGContextSaveGState(gCtx);
                        CGContextSetFillColor(gCtx, &gRuleColor.color.red);
					}
                    CGContextFillRect(gCtx, r);
                    if (gRuleColor.override)
                        CGContextRestoreGState(gCtx);
                    if (gTrackingBoxes) {
						box	b = {
							dvi.h,
							dvi.v,
							dvi.h + wd,
							dvi.v - ht };
						mergeBox(b);
                    }
				}
				if (cmd == DVI_SETRULE)
					dvi.h += wd;
				break;
			
			case DVI_SET4:
				++k;
			case DVI_SET3:
				++k;
			case DVI_SET2:
				++k;
			case DVI_SET1:
				u = readUnsigned(xdv, k);
				setChar(u, true);
				break;
			
			case DVI_PUT4:
				++k;
			case DVI_PUT3:
				++k;
			case DVI_PUT2:
				++k;
			case DVI_PUT1:
				u = readUnsigned(xdv, k);
				setChar(u, false);
				break;
			
			case DVI_PUSH:
				stack.push_back(dvi);
				++gDviLevel;
				break;

			case DVI_POP:
				if (gDviLevel == gTagLevel)
					flushAnnotBox();
				--gDviLevel;
				dvi = stack.back();
				stack.pop_back();
				break;
			
			case DVI_RIGHT4:
				++k;
			case DVI_RIGHT3:
				++k;
			case DVI_RIGHT2:
				++k;
			case DVI_RIGHT1:
				s = readSigned(xdv, k);
				dvi.h += s;
				break;
			
			case DVI_DOWN4:
				++k;
			case DVI_DOWN3:
				++k;
			case DVI_DOWN2:
				++k;
			case DVI_DOWN1:
				s = readSigned(xdv, k);
				dvi.v += s;
				break;
			
			case DVI_W4:
				++k;
			case DVI_W3:
				++k;
			case DVI_W2:
				++k;
			case DVI_W1:
				s = readSigned(xdv, k);
				dvi.w = s;
			case DVI_W0:
				dvi.h += dvi.w;
				break;
				
			case DVI_X4:
				++k;
			case DVI_X3:
				++k;
			case DVI_X2:
				++k;
			case DVI_X1:
				s = readSigned(xdv, k);
				dvi.x = s;
			case DVI_X0:
				dvi.h += dvi.x;
				break;
				
			case DVI_Y4:
				++k;
			case DVI_Y3:
				++k;
			case DVI_Y2:
				++k;
			case DVI_Y1:
				s = readSigned(xdv, k);
				dvi.y = s;
			case DVI_Y0:
				dvi.v += dvi.y;
				break;
				
			case DVI_Z4:
				++k;
			case DVI_Z3:
				++k;
			case DVI_Z2:
				++k;
			case DVI_Z1:
				s = readSigned(xdv, k);
				dvi.z = s;
			case DVI_Z0:
				dvi.v += dvi.z;
				break;
			
			case DVI_GLYPH:
				ensurePageStarted();
				u = readUnsigned(xdv, 2);
				s = readSigned(xdv, 4);
				doSetGlyph(u, s);
				break;
			
			case DVI_NATIVE:
				ensurePageStarted();
				doSetNative(xdv);
				break;
			
			case DVI_GLYPH_ARRAY:
				ensurePageStarted();
				doGlyphArray(xdv);
				break;
			
            case DVI_PIC_FILE:
				ensurePageStarted();
                doPicFile(xdv, false);
                break;
                            
            case DVI_PDF_FILE:
				ensurePageStarted();
                doPicFile(xdv, true);
                break;
                        
			case DVI_FNTDEF4:
				++k;
			case DVI_FNTDEF3:
				++k;
			case DVI_FNTDEF2:
				++k;
			case DVI_FNTDEF1:
				doFontDef(xdv, k);
				break;
				
			case DVI_NATIVE_FONT_DEF:
                doNativeFontDef(xdv);
				break;
		}
	}

	ensurePageStarted();	// needed for completely blank pages!

	if (sVerbose) {
		fprintf(stderr, "]%s", (gPageIndex % 10) == 0 ? "\n" : "");
	}

ABORT_PAGE:
    
	if (gPageStarted) {
		CGContextEndPage(gCtx);
		CGContextRestoreGState(gCtx);
	}
	
	gPageStarted = false;
}

static void
processAllPages(FILE* xdv)
{
	gPageIndex = 0;
	unsigned int	cmd;
	while ((cmd = readUnsigned(xdv, 1)) != DVI_POST) {
		switch (cmd) {
			case DVI_BOP:
				processPage(xdv);
				break;
				
			case DVI_NOP:
				break;
			
			default:
				fprintf(stderr, "\n*** unexpected DVI command: %d\n", cmd);
                exit(1);
		}
	}
}

const char* progName;
static void
usage()
{
    fprintf(stderr, "usage: %s [-m mag] [-p papersize[:landscape]] [-v] [-o pdfFile] xdvFile\n", progName);
	fprintf(stderr, "    papersize values: ");
	paperSizeRec*	ps = &gPaperSizes[0];
	while (ps->name != 0) {
		fprintf(stderr, "%s/", ps->name);
		++ps;
	}
	fprintf(stderr, "wd,ht [in 'big' points or with explicit units]\n");
}

int
xdv2pdf(int argc, char** argv)
{
	OSStatus			status;
    
    progName = argv[0];
    
	gMagScale = 1.0;
	gRuleColor.color = kBlackColor;
	gRuleColor.override = false;
	gTextColor = gTextColor;

	gTagLevel = -1;
	
	double_t	paperWd = 0.0;
	double_t	paperHt = 0.0;

    int	ch;
    while ((ch = getopt(argc, argv, "o:p:m:d:hv" /*r:*/)) != -1) {
        switch (ch) {
            case 'o':
                {
                    CFStringRef	outFileName = CFStringCreateWithCString(kCFAllocatorDefault, optarg, kCFStringEncodingUTF8);
                    gSaveURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, outFileName, kCFURLPOSIXPathStyle, false);
                    CFRelease(outFileName);
                }
                break;
            
            case 'p':
				if (!getPaperSize(optarg, paperWd, paperHt)) {
					fprintf(stderr, "*** unknown paper name: %s\n", optarg);
					exit(1);
				}
                break;
                
            case 'm':
            	sMag = atoi(optarg);
            	break;
            
            case 'd':
            	kpathsea_debug |= atoi(optarg);
            	break;
            
            case 'v':
            	sVerbose = true;
            	break;
            
            case 'h':
                usage();
                exit(0);
        }
    }

	if ((paperWd == 0.0) || (paperHt == 0.0)) {
		// get default paper size from printing system
		PMRect				paperRect = { 0, 0, 792, 612 };
		PMPrintSession		printSession;
		PMPrintSettings		printSettings;
		PMPageFormat		pageFormat;
		status = PMCreateSession(&printSession);
		if (status == noErr) {
			status = PMCreatePrintSettings(&printSettings);
			if (status == noErr) {
				status = PMSessionDefaultPrintSettings(printSession, printSettings);
				status = PMCreatePageFormat(&pageFormat);
				if (status == noErr) {
					status = PMSessionDefaultPageFormat(printSession, pageFormat);
					PMGetUnadjustedPaperRect(pageFormat, &paperRect);
					status = PMRelease(pageFormat);
				}
				status = PMRelease(printSettings);
			}
			status = PMRelease(printSession);
		}
		paperWd = paperRect.right - paperRect.left;
		paperHt = paperRect.bottom - paperRect.top;
	}

    // set the media box for PDF generation
    gMediaBox = CGRectMake(0, 0, paperWd, paperHt);
    
    argc -= optind;
    argv += optind;
    if (argc == 1 && gSaveURL == 0) {
        CFStringRef	inFileName = CFStringCreateWithCString(kCFAllocatorDefault, argv[0], CFStringGetSystemEncoding());
        CFURLRef	inURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inFileName, kCFURLPOSIXPathStyle, false);
        CFRelease(inFileName);
        CFURLRef	tmpURL = CFURLCreateCopyDeletingPathExtension(kCFAllocatorDefault, inURL);
        CFRelease(inURL);
        gSaveURL = CFURLCreateCopyAppendingPathExtension(kCFAllocatorDefault, tmpURL, CFSTR("pdf"));
        CFRelease(tmpURL);
    }

    if (argc > 1 || gSaveURL == 0) {
        usage();
        exit(1);
    }
    
	ATSUCreateTextLayout(&sLayout);
//	ATSUFontFallbacks fallbacks;
//	status = ATSUCreateFontFallbacks(&fallbacks);
//	status = ATSUSetObjFontFallbacks(fallbacks, 0, 0, /*kATSULastResortOnlyFallback*/kATSUDefaultFontFallbacks);
//	ATSUAttributeTag		tag = kATSULineFontFallbacksTag;
//	ByteCount				valueSize = sizeof(fallbacks);
//	ATSUAttributeValuePtr	value = &fallbacks;
//	status = ATSUSetLayoutControls(sLayout, 1, &tag, &valueSize, &value);

// this doesn't seem to be working for me....
//	status = ATSUSetTransientFontMatching(sLayout, true);

	FILE*	xdv;
    if (argc == 1)
        xdv = fopen(argv[0], "r");
    else
        xdv = stdin;

	if (xdv != NULL) {
		// read the preamble
		unsigned	b = readUnsigned(xdv, 1);
		if (b != DVI_PRE) { fprintf(stderr, "*** bad XDV file: DVI_PRE not found, b=%d\n", b); exit(1); }
		b = readUnsigned(xdv, 1);
		if (b != XDV_ID) { fprintf(stderr, "*** bad XDV file: version=%d, expected %d\n", b, XDV_ID); exit(1); }
		(void)readUnsigned(xdv, 4);	// num
		(void)readUnsigned(xdv, 4);	// den
		if (sMag == 0)
			sMag = readUnsigned(xdv, 4);	// sMag
		else
			(void)readUnsigned(xdv, 4);

		unsigned numBytes = readUnsigned(xdv, 1);	// length of output comment
        UInt8* bytes = new UInt8[numBytes];
        fread(bytes, 1, numBytes, xdv);

        CFStringRef	keys[2] =   { CFSTR("Creator") , CFSTR("Title") /*, CFSTR("Author")*/ };
        CFStringRef values[2] = { CFSTR("xdv2pdf") , 0 /*, 0*/ };
        values[1] = CFStringCreateWithBytes(kCFAllocatorDefault, bytes, numBytes, CFStringGetSystemEncoding(), false);
        delete[] bytes;
//        values[2] = CFStringCreateWithCString(kCFAllocatorDefault, getlogin(), CFStringGetSystemEncoding());
        CFDictionaryRef	auxInfo = CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)values, sizeof(keys) / sizeof(keys[0]),
                &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFRelease(values[1]);
//        CFRelease(values[2]);

        // create PDF graphics context
        gCtx = CGPDFContextCreateWithURL(gSaveURL, &gMediaBox, auxInfo);

		// set up the TextLayout to use this graphics context
		ByteCount				iSize = sizeof(CGContextRef);
		ATSUAttributeTag		iTag = kATSUCGContextTag;
		ATSUAttributeValuePtr	iValuePtr = &gCtx;
		ATSUSetLayoutControls(sLayout, 1, &iTag, &iSize, &iValuePtr); 
	
		// handle magnification
		if (sMag != 1000) {
			gMagScale = sMag / 1000.0;
			kScr2Dvi /= gMagScale;
			kDvi2Scr *= gMagScale;
			CGContextScaleCTM(gCtx, gMagScale, gMagScale);
		}

		// draw all the pages
        processAllPages(xdv);

        CGContextRelease(gCtx);

		while (getc(xdv) != EOF)
			;
		fclose(xdv);
	}
	
    ATSUDisposeTextLayout(sLayout);
    
	if (gPdfMarkFile != NULL) {
		fclose(gPdfMarkFile);
		char	pdfPath[_POSIX_PATH_MAX+1];
		Boolean	gotPath = CFURLGetFileSystemRepresentation(gSaveURL, true, (UInt8*)pdfPath, _POSIX_PATH_MAX);
		CFRelease(gSaveURL);
#if 0
		if (gotPath) {
			char*	mergeMarks = "xdv2pdf_mergemarks";
			execlp(mergeMarks, mergeMarks, pdfPath, gPdfMarkPath, 0);	// should not return!
			status = errno;
		}
		fprintf(stderr, "*** failed to run xdv2pdf_mergemarks: status = %d\n", status);
#else
		if (gotPath) {
			char*	mergeMarks = "xdv2pdf_mergemarks";
			char	cmd[_POSIX_PATH_MAX*2 + 100];
			sprintf(cmd, "%s \"%s\" \"%s\"", mergeMarks, pdfPath, gPdfMarkPath);
			status = system(cmd);
		}
		else
			status = fnfErr;
		if (status != 0)
			fprintf(stderr, "*** failed to run xdv2pdf_mergemarks: status = %d\n", status);
#endif
	}
	else
		CFRelease(gSaveURL);

	return status;
}
