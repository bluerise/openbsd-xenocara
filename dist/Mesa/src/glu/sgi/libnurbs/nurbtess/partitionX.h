/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */
/*
** $Header: /Volumes/cvs/xenocara/dist/Mesa/src/glu/sgi/libnurbs/nurbtess/Attic/partitionX.h,v 1.2 2008/11/02 14:58:11 matthieu Exp $
*/

#ifndef _PARTITIONX_H
#define _PARTITIONX_H

#include "directedLine.h"

Int isCuspX(directedLine *v);
Int isReflexX(directedLine *v);
Int cuspTypeX(directedLine *v);

//assuming the array of ret_interior_cusps has been allocated
void findInteriorCuspsX(directedLine* polygon, Int& ret_n_interior_cusps,
		       directedLine** ret_interior_cusps);

Int numInteriorCuspsX(directedLine* polygon);

/*a single polygon with a single cusp
 *return the diagonal vertex corresponding to this cusp
 */
directedLine* findDiagonal_singleCuspX(directedLine* cusp);

#endif

