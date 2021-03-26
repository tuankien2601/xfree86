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

#include	"s3.h"
#include	"s3draw.h"

#include	"Xmd.h"
#include	"gcstruct.h"
#include	"scrnintstr.h"
#include	"pixmapstr.h"
#include	"regionstr.h"
#include	"mistruct.h"
#include	"fontstruct.h"
#include	"dixfontstr.h"
#include	"migc.h"

/*
 * Common op groups.  Common assumptions:
 *
 *  lineWidth	0
 *  lineStyle	LineSolid
 *  fillStyle	FillSolid
 *  rop		GXcopy
 *  font	<= 32 pixels wide
 */

/* TE font, >= 4 pixels wide, one clip rectangle */
static GCOps	s3TEOps1Rect = {
    s3FillSpans,
    fbSetSpans,
    fbPutImage,
    s3CopyArea,
    s3CopyPlane,
    fbPolyPoint,
    s3Polylines,
    s3PolySegment,
    miPolyRectangle,
    fbPolyArc,
    s3FillPoly1Rect,
    s3PolyFillRect,
    s3PolyFillArcSolid,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    s3ImageTEGlyphBlt,
    s3PolyTEGlyphBlt,
    fbPushPixels,
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

extern GCOps	    fbGCOps;

/* Non TE font, one clip rectangle */
static GCOps	s3NonTEOps1Rect = {
    s3FillSpans,
    fbSetSpans,
    fbPutImage,
    s3CopyArea,
    s3CopyPlane,
    fbPolyPoint,
    s3Polylines,
    s3PolySegment,
    miPolyRectangle,
    fbPolyArc,
    s3FillPoly1Rect,
    s3PolyFillRect,
    s3PolyFillArcSolid,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    s3ImageGlyphBlt,
    s3PolyGlyphBlt,
    fbPushPixels
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

/* TE font, != 1 clip rect (including 0) */
static GCOps	s3TEOps = {
    s3FillSpans,
    fbSetSpans,
    fbPutImage,
    s3CopyArea,
    s3CopyPlane,
    fbPolyPoint,
    s3Polylines,
    s3PolySegment,
    miPolyRectangle,
    fbPolyArc,
    miFillPolygon,
    s3PolyFillRect,
    s3PolyFillArcSolid,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    s3ImageTEGlyphBlt,
    s3PolyTEGlyphBlt,
    fbPushPixels
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

/* Non TE font, != 1 clip rect (including 0) */
static GCOps	s3NonTEOps = {
    s3FillSpans,
    fbSetSpans,
    fbPutImage,
    s3CopyArea,
    s3CopyPlane,
    fbPolyPoint,
    s3Polylines,
    s3PolySegment,
    miPolyRectangle,
    fbPolyArc,
    miFillPolygon,
    s3PolyFillRect,
    s3PolyFillArcSolid,
    miPolyText8,
    miPolyText16,
    miImageText8,
    miImageText16,
    s3ImageGlyphBlt,
    s3PolyGlyphBlt,
    fbPushPixels
#ifdef NEED_LINEHELPER
    ,NULL
#endif
};

static GCOps *
s3MatchCommon (DrawablePtr pDraw, GCPtr pGC, FbGCPrivPtr fbPriv)
{
    KdScreenPriv (pDraw->pScreen);

    if (!REGION_NOTEMPTY(pDraw->pScreen,fbGetCompositeClip(pGC)))
    {
	DRAW_DEBUG ((DEBUG_CLIP, "Empty composite clip, clipping all ops"));
	return &kdNoopOps;
    }

    if (pDraw->type != DRAWABLE_WINDOW)
	return (GCOps *) &fbGCOps;
    
    if (pGC->lineWidth != 0)
	return 0;
    if (pGC->lineStyle != LineSolid)
	return 0;
    if (pGC->fillStyle != FillSolid)
	return 0;
    if (fbPriv->and != 0)
	return 0;
    if (pGC->font)
    {
	if (TERMINALFONT(pGC->font))
	{
	    if (fbPriv->oneRect)
		return &s3TEOps1Rect;
	    else
		return &s3TEOps;
	}
	else
	{
	    if (fbPriv->oneRect)
		return &s3NonTEOps1Rect;
	    else
		return &s3NonTEOps;
	}
    }
    return 0;
}

void
s3ValidateGC (GCPtr pGC, Mask changes, DrawablePtr pDrawable)
{
    int		new_type;	/* drawable type has changed */
    int		new_onerect;	/* onerect value has changed */
    int		new_origin;
    
    /* flags for changing the proc vector */
    FbGCPrivPtr fbPriv;
    s3PrivGCPtr	s3Priv;
    int		oneRect;
    
    fbPriv = fbGetGCPrivate(pGC);
    s3Priv = s3GetGCPrivate(pGC);

    new_type = FALSE;
    new_onerect = FALSE;
    new_origin = FALSE;

    /*
     * If the type of drawable has changed, fix up accelerated functions
     */
    if (s3Priv->type != pDrawable->type)
    {
	new_type = TRUE;
	s3Priv->type = pDrawable->type;
    }

    /*
     * Check tile/stipple origin
     */
    if (pGC->lastWinOrg.x != pDrawable->x || pGC->lastWinOrg.y != pDrawable->y)
	new_origin = TRUE;
    
    /*
     * Call down to FB to set clip list and rrop values
     */
    oneRect = fbPriv->oneRect;
    
    fbValidateGC (pGC, changes, pDrawable);
    
    if (oneRect != fbPriv->oneRect)
	new_onerect = TRUE;
    
    /*
     * Check accelerated pattern if necessary
     */
    if (changes & (GCFillStyle|GCStipple|GCTile))
	s3CheckGCFill (pGC);
    else if (s3Priv->pPattern && 
	     (new_origin || changes & (GCTileStipXOrigin|GCTileStipYOrigin)))
	s3MoveGCFill (pGC);

    /*
     * Try to match common vector
     */
    
    if (new_type || new_onerect ||
	(changes & (GCLineWidth|GCLineStyle|GCFillStyle|
		    GCFont|GCForeground|GCFunction|GCPlaneMask)))
    {
	GCOps	*newops;

	if (newops = s3MatchCommon (pDrawable, pGC, fbPriv))
 	{
	    if (pGC->ops->devPrivate.val)
		miDestroyGCOps (pGC->ops);
	    pGC->ops = newops;
	    return;
	}
    }
    
    /*
     * No common vector matched, create private ops vector and
     * fill it in
     */
    if (!pGC->ops->devPrivate.val)
    {
	/*
	 * Switch from noop vector by first switching to fb
	 * vector and fixing it up
	 */
	if (pGC->ops == &kdNoopOps)
	{
	    pGC->ops = (GCOps *) &fbGCOps;
	    new_type = TRUE;
	}
        pGC->ops = miCreateGCOps (pGC->ops);
        pGC->ops->devPrivate.val = 1;
    }

    /*
     * Fills
     */
    if (new_type || (changes & (GCFillStyle|GCTile|GCStipple)))
    {
        pGC->ops->FillSpans = fbFillSpans;
	pGC->ops->PolyFillRect = fbPolyFillRect;
	if (s3Priv->type == DRAWABLE_WINDOW &&
	    (pGC->fillStyle != FillTiled || s3Priv->pPattern))
	{
	    pGC->ops->FillSpans = s3FillSpans;
	    pGC->ops->PolyFillRect = s3PolyFillRect;
	}
    }

    /*
     * Blt
     */
    if (new_type)
    {
	pGC->ops->CopyArea = fbCopyArea;
	pGC->ops->CopyPlane = fbCopyPlane;
	if (s3Priv->type == DRAWABLE_WINDOW)
	{
	    pGC->ops->CopyArea = s3CopyArea;
	    pGC->ops->CopyPlane = s3CopyPlane;
	}
    }
    
    /*
     * Lines
     */
    if (new_type || (changes & (GCLineStyle|GCLineWidth|GCFillStyle)))
    {
	pGC->ops->Polylines = fbPolyLine;
	pGC->ops->PolySegment = fbPolySegment;
	if (pGC->lineStyle == LineSolid &&
	    pGC->lineWidth == 0 &&
	    pGC->fillStyle == FillSolid &&
	    s3Priv->type == DRAWABLE_WINDOW)
	{
	    pGC->ops->Polylines = s3Polylines;
	    pGC->ops->PolySegment = s3PolySegment;
	}
    }

    /*
     * Polygons
     */
    if (new_type || new_onerect || (changes & (GCFillStyle)))
    {
	pGC->ops->FillPolygon = miFillPolygon;
	if (s3Priv->type == DRAWABLE_WINDOW &&
	    fbPriv->oneRect && 
	    pGC->fillStyle == FillSolid)
	{
	    pGC->ops->FillPolygon = s3FillPoly1Rect;
	}
    }
	
    /*
     * Filled arcs
     */
    if (new_type || (changes & GCFillStyle))
    {
	pGC->ops->PolyFillArc = fbPolyFillArc;
	if (s3Priv->type == DRAWABLE_WINDOW &&
	    pGC->fillStyle == FillSolid)
	{
	    pGC->ops->PolyFillArc = s3PolyFillArcSolid;
	}
    }
    
    /*
     * Text
     */
    if (new_type || (changes & (GCFont|GCFillStyle)))
    {
	pGC->ops->PolyGlyphBlt = fbPolyGlyphBlt;
	pGC->ops->ImageGlyphBlt = fbImageGlyphBlt;
	if (s3Priv->type == DRAWABLE_WINDOW && pGC->font)
	{
	    if (pGC->fillStyle == FillSolid)
	    {
		if (TERMINALFONT(pGC->font))
		    pGC->ops->PolyGlyphBlt = s3PolyTEGlyphBlt;
		else
		    pGC->ops->PolyGlyphBlt = s3PolyGlyphBlt;
	    }
	    if (TERMINALFONT(pGC->font))
		pGC->ops->ImageGlyphBlt = s3ImageTEGlyphBlt;
	    else
		pGC->ops->ImageGlyphBlt = s3ImageGlyphBlt;
        }
    }    
}
