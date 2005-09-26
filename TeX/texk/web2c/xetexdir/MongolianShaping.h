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
 *
 * Based on ICU code, (C) Copyright IBM Corp. 1998-2004 - All Rights Reserved
 *
 */

#ifndef __MONGOLIANSHAPING_H
#define __MONGOLIANSHAPING_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class MongolianShaping /* not : public UObject because all methods are static */ {
public:
    // shaping bit masks
    enum ShapingBitMasks
    {
        MASK_SHAPE_RIGHT    = 1, // if this bit set, shapes to right
        MASK_SHAPE_LEFT     = 2, // if this bit set, shapes to left
        MASK_TRANSPARENT    = 4, // if this bit set, is transparent (ignore other bits)
        MASK_NOSHAPE        = 8  // if this bit set, don't shape this char, i.e. tatweel
    };

    // shaping values
    enum ShapeTypeValues
    {
        ST_NONE         = 0,
        ST_RIGHT        = MASK_SHAPE_RIGHT,
        ST_LEFT         = MASK_SHAPE_LEFT,
        ST_DUAL         = MASK_SHAPE_RIGHT | MASK_SHAPE_LEFT,
        ST_TRANSPARENT  = MASK_TRANSPARENT,
        ST_NOSHAPE_DUAL = MASK_NOSHAPE | ST_DUAL,
        ST_NOSHAPE_NONE = MASK_NOSHAPE
    };

    typedef le_int32 ShapeType;

    static void shape(const LEUnicode *chars, le_int32 offset, le_int32 charCount, le_int32 charMax,
                      le_bool rightToLeft, LEGlyphStorage &glyphStorage);

    static const LETag *getFeatureOrder();

    static const le_uint8 glyphSubstitutionTable[];
  //static le_uint8 ligatureSubstitutionSubtable[];
    static const le_uint8 glyphDefinitionTable[];

private:
    // forbid instantiation
    MongolianShaping();

    static const LETag tagArray[];

    static ShapeType getShapeType(LEUnicode c);

    static const ShapeType shapeTypes[];

    static void adjustTags(le_int32 outIndex, le_int32 shapeOffset, LEGlyphStorage &glyphStorage); 
};

U_NAMESPACE_END
#endif