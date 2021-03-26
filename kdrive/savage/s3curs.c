/*
 * $Id$
 *
 * Copyright 1999 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */
/* $XFree86: $ */

#include "s3.h"
#include "cursorstr.h"

#define SetupCursor(s)	    KdScreenPriv(s); \
			    s3CardInfo(pScreenPriv); \
			    s3ScreenInfo(pScreenPriv); \
			    S3Ptr s3 = s3c->s3; \
			    S3Vga *s3vga = &s3c->s3vga; \
			    S3Cursor *pCurPriv = &s3s->cursor

static void
_s3MoveCursor (ScreenPtr pScreen, int x, int y)
{
    SetupCursor(pScreen);
    CARD8   xlow, xhigh, ylow, yhigh;
    CARD8   xoff, yoff;

    DRAW_DEBUG ((DEBUG_CURSOR, "s3MoveCursor %d %d", x, y));
    
    x -= pCurPriv->xhot;
    xoff = 0;
    if (x < 0)
    {
	xoff = -x;
	x = 0;
    }
    y -= pCurPriv->yhot;
    yoff = 0;
    if (y < 0)
    {
	yoff = -y;
	y = 0;
    }
    xlow = (CARD8) x;
    xhigh = (CARD8) (x >> 8);
    ylow = (CARD8) y;
    yhigh = (CARD8) (y >> 8);
    
    
    /* This is the recommended order to move the cursor */
    
    s3SetImm (s3vga, s3_cursor_xhigh, xhigh);
    s3SetImm (s3vga, s3_cursor_xlow, xlow);
    s3SetImm (s3vga, s3_cursor_ylow, ylow);
    s3SetImm (s3vga, s3_cursor_xoff, xoff);
    s3SetImm (s3vga, s3_cursor_yoff, yoff);
    s3SetImm (s3vga, s3_cursor_yhigh, yhigh);
    
    DRAW_DEBUG ((DEBUG_CURSOR, "s3MoveCursor done"));
}

static void
s3MoveCursor (ScreenPtr pScreen, int x, int y)
{
    SetupCursor (pScreen);
    
    if (!pCurPriv->has_cursor)
	return;
    
    if (!pScreenPriv->enabled)
	return;
    
    _s3MoveCursor (pScreen, x, y);
}

static void
s3AllocCursorColors (ScreenPtr pScreen)
{
    SetupCursor (pScreen);
    CursorPtr	    pCursor = pCurPriv->pCursor;
    xColorItem	    sourceColor, maskColor;
    
    /*
     * Set these to an invalid pixel value so that
     * when the store colors comes through, the cursor
     * won't get recolored
     */
    pCurPriv->source = ~0;
    pCurPriv->mask = ~0;
    /* 
     * XXX S3 bug workaround; s3 chip doesn't use RGB values from
     * the cursor color registers as documented, rather it uses
     * them to index the DAC.  This is in the errata though.
     */
    sourceColor.red = pCursor->foreRed;
    sourceColor.green = pCursor->foreGreen;
    sourceColor.blue = pCursor->foreBlue;
    FakeAllocColor(pScreenPriv->pInstalledmap, &sourceColor);
    maskColor.red = pCursor->backRed;
    maskColor.green = pCursor->backGreen;
    maskColor.blue = pCursor->backBlue;
    FakeAllocColor(pScreenPriv->pInstalledmap, &maskColor);
    FakeFreeColor(pScreenPriv->pInstalledmap, sourceColor.pixel);
    FakeFreeColor(pScreenPriv->pInstalledmap, maskColor.pixel);
    
    pCurPriv->source = sourceColor.pixel;
    pCurPriv->mask = maskColor.pixel;
    switch (pScreenPriv->screen->bitsPerPixel) {
    case 4:
	pCurPriv->source |= pCurPriv->source << 4;
	pCurPriv->mask |= pCurPriv->mask << 4;
    case 8:
	pCurPriv->source |= pCurPriv->source << 8;
	pCurPriv->mask |= pCurPriv->mask << 8;
    case 16:
	pCurPriv->source |= pCurPriv->source << 16;
	pCurPriv->mask |= pCurPriv->mask << 16;
    }
}

static void
_s3SetCursorColors (ScreenPtr pScreen)
{
    SetupCursor (pScreen);
    /* set foreground */
    /* Reset cursor color stack pointers */
    (void) s3GetImm (s3vga, s3_cursor_enable);
    s3SetImm (s3vga, s3_cursor_fg, pCurPriv->source);
    s3SetImm (s3vga, s3_cursor_fg, pCurPriv->source >> 8);
    s3SetImm (s3vga, s3_cursor_fg, pCurPriv->source >> 16);
	
    /* set background */
    /* Reset cursor color stack pointers */
    (void) s3GetImm (s3vga, s3_cursor_enable);
    s3SetImm (s3vga, s3_cursor_bg, pCurPriv->mask);
    s3SetImm (s3vga, s3_cursor_bg, pCurPriv->mask >> 8);
    s3SetImm (s3vga, s3_cursor_bg, pCurPriv->mask >> 16);
}
    
void
s3RecolorCursor (ScreenPtr pScreen, int ndef, xColorItem *pdef)
{
    SetupCursor (pScreen);
    CursorPtr	    pCursor = pCurPriv->pCursor;
    xColorItem	    sourceColor, maskColor;

    if (!pCurPriv->has_cursor || !pCursor)
	return;
    
    if (!pScreenPriv->enabled)
	return;
    
    if (pdef)
    {
	while (ndef)
	{
	    if (pdef->pixel == pCurPriv->source || 
		pdef->pixel == pCurPriv->mask)
		break;
	    ndef--;
	}
	if (!ndef)
	    return;
    }
    s3AllocCursorColors (pScreen);
    _s3SetCursorColors (pScreen);
}
    
static void
s3LoadCursor (ScreenPtr pScreen, int x, int y)
{
    SetupCursor(pScreen);
    CursorPtr	    pCursor = pCurPriv->pCursor;
    CursorBitsPtr   bits = pCursor->bits;
    int	w, h;
    unsigned char   r[2], g[2], b[2];
    unsigned short  *ram, *msk, *mskLine, *src, *srcLine;
    unsigned short  and, xor;
    int		    i, j;
    int		    cursor_address;
    int		    wsrc;
    unsigned char   ramdac_control_;

    /*
     * Allocate new colors
     */
    s3AllocCursorColors (pScreen);
    
    pCurPriv->pCursor = pCursor;
    pCurPriv->xhot = pCursor->bits->xhot;
    pCurPriv->yhot = pCursor->bits->yhot;
    
    /*
     * Stick new image into cursor memory
     */
    ram = (unsigned short *) s3s->cursor_base;
    mskLine = (unsigned short *) bits->mask;
    srcLine = (unsigned short *) bits->source;

    h = bits->height;
    if (h > S3_CURSOR_HEIGHT)
	h = S3_CURSOR_HEIGHT;

    wsrc = BitmapBytePad(bits->width) / 2;        /* words per line */

    for (i = 0; i < S3_CURSOR_HEIGHT; i++) {
	msk = mskLine;
	src = srcLine;
	mskLine += wsrc;
	srcLine += wsrc;
	for (j = 0; j < S3_CURSOR_WIDTH / 16; j++) {

	    unsigned short  m, s;

	    if (i < h && j < wsrc) 
	    {
		m = *msk++;
		s = *src++;
		xor = m & s;
		and = ~m;
	    }
	    else
	    {
		and = 0xffff;
		xor = 0x0000;
	    }
		
	    S3InvertBits16(and);
	    *ram++ = and;
	    S3InvertBits16(xor);
	    *ram++ = xor;
	}
    }
    
    _s3WaitIdle (s3);
    
    /* Set new color */
    _s3SetCursorColors (pScreen);
     
    /* Enable the cursor */
    s3SetImm (s3vga, s3_cursor_enable, 1);
    
    /* Wait for VRetrace to make sure the position is read */
    _s3WaitVRetrace (s3);
    
    /* Move to new position */
    _s3MoveCursor (pScreen, x, y);
}

static void
s3UnloadCursor (ScreenPtr pScreen)
{
    SetupCursor (pScreen);

    /* Disable cursor */
    s3SetImm (s3vga, s3_cursor_enable, 0);
}

static Bool
s3RealizeCursor (ScreenPtr pScreen, CursorPtr pCursor)
{
    SetupCursor(pScreen);

    if (!pScreenPriv->enabled)
	return TRUE;
    
    /* miRecolorCursor does this */
    if (pCurPriv->pCursor == pCursor)
    {
	if (pCursor)
	{
	    int	    x, y;
	    
	    miPointerPosition (&x, &y);
	    s3LoadCursor (pScreen, x, y);
	}
    }
    return TRUE;
}

static Bool
s3UnrealizeCursor (ScreenPtr pScreen, CursorPtr pCursor)
{
    return TRUE;
}

static void
s3SetCursor (ScreenPtr pScreen, CursorPtr pCursor, int x, int y)
{
    SetupCursor(pScreen);

    pCurPriv->pCursor = pCursor;
    
    if (!pScreenPriv->enabled)
	return;
    
    if (pCursor)
	s3LoadCursor (pScreen, x, y);
    else
	s3UnloadCursor (pScreen);
}

miPointerSpriteFuncRec s3PointerSpriteFuncs = {
    s3RealizeCursor,
    s3UnrealizeCursor,
    s3SetCursor,
    s3MoveCursor,
};

static void
s3QueryBestSize (int class, 
		 unsigned short *pwidth, unsigned short *pheight, 
		 ScreenPtr pScreen)
{
    SetupCursor (pScreen);

    switch (class)
    {
    case CursorShape:
	if (*pwidth > pCurPriv->width)
	    *pwidth = pCurPriv->width;
	if (*pheight > pCurPriv->height)
	    *pheight = pCurPriv->height;
	if (*pwidth > pScreen->width)
	    *pwidth = pScreen->width;
	if (*pheight > pScreen->height)
	    *pheight = pScreen->height;
	break;
    default:
	fbQueryBestSize (class, pwidth, pheight, pScreen);
	break;
    }
}

Bool
s3CursorInit (ScreenPtr pScreen)
{
    SetupCursor (pScreen);

    if (!s3s->cursor_base)
    {
	DRAW_DEBUG ((DEBUG_CURSOR,"Not enough screen memory for cursor %d", s3d->memory));
	pCurPriv->has_cursor = FALSE;
	return FALSE;
    }
    
    pCurPriv->width = S3_CURSOR_WIDTH;
    pCurPriv->height= S3_CURSOR_HEIGHT;
    pScreen->QueryBestSize = s3QueryBestSize;
    miPointerInitialize (pScreen,
			 &s3PointerSpriteFuncs,
			 &kdPointerScreenFuncs,
			 FALSE);
    pCurPriv->has_cursor = TRUE;
    pCurPriv->pCursor = NULL;
    return TRUE;
}

void
s3CursorEnable (ScreenPtr pScreen)
{
    SetupCursor (pScreen);

    DRAW_DEBUG ((DEBUG_INIT, "s3CursorEnable"));
    if (pCurPriv->has_cursor)
    {
	if (pCurPriv->pCursor)
	{
	    int	    x, y;
	    
	    miPointerPosition (&x, &y);
	    s3LoadCursor (pScreen, x, y);
	}
	else
	    s3UnloadCursor (pScreen);
    }
}

void
s3CursorDisable (ScreenPtr pScreen)
{
    SetupCursor (pScreen);

    if (!pScreenPriv->enabled)
	return;
    
    if (pCurPriv->has_cursor)
    {
	if (pCurPriv->pCursor)
	{
	    s3UnloadCursor (pScreen);
	}
    }
}

void
s3CursorFini (ScreenPtr pScreen)
{
    SetupCursor (pScreen);

    pCurPriv->pCursor = NULL;
}
