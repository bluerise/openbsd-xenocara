/*
 * Copyright (c) 2005 ASPEED Technology Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the authors not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86cmap.h"
#include "compiler.h"
#include "mibstore.h"
#include "vgaHW.h"
#include "mipointer.h"
#include "micmap.h"

#include "fb.h"
#include "regionstr.h"
#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "vbe.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

/* framebuffer offscreen manager */
#include "xf86fbman.h"

/* include xaa includes */
#include "xaa.h"
#include "xaarop.h"

/* H/W cursor support */
#include "xf86Cursor.h"

/* Driver specific headers */
#include "ast.h"

#ifdef	Accel_2D
/* ROP Translation Table */
int ASTXAACopyROP[16] =
{
   ROP_0,               /* GXclear */
   ROP_DSa,             /* GXand */
   ROP_SDna,            /* GXandReverse */
   ROP_S,               /* GXcopy */
   ROP_DSna,            /* GXandInverted */
   ROP_D,               /* GXnoop */
   ROP_DSx,             /* GXxor */
   ROP_DSo,             /* GXor */
   ROP_DSon,            /* GXnor */
   ROP_DSxn,            /* GXequiv */
   ROP_Dn,              /* GXinvert*/
   ROP_SDno,            /* GXorReverse */
   ROP_Sn,              /* GXcopyInverted */
   ROP_DSno,            /* GXorInverted */
   ROP_DSan,            /* GXnand */
   ROP_1                /* GXset */
};

int ASTXAAPatternROP[16]=
{
   ROP_0,
   ROP_DPa,
   ROP_PDna,
   ROP_P,
   ROP_DPna,
   ROP_D,
   ROP_DPx,
   ROP_DPo,
   ROP_DPon,
   ROP_PDxn,
   ROP_Dn,
   ROP_PDno,
   ROP_Pn,
   ROP_DPno,
   ROP_DPan,
   ROP_1
};

/* extern function */
extern void vWaitEngIdle(ScrnInfoPtr pScrn, ASTRecPtr pAST);
extern UCHAR *pjRequestCMDQ(ASTRecPtr pAST, ULONG ulDataLen);
extern Bool bGetLineTerm(_LINEInfo *LineInfo, LINEPARAM *dsLineParam);

/* Prototype type declaration */
Bool ASTAccelInit(ScreenPtr pScreen);
static void ASTSync(ScrnInfoPtr pScrn);
static void ASTSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
                                          int xdir, int ydir, int rop,
                                          unsigned int planemask, int trans_color);
static void ASTSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, int x2,
                                            int y2, int w, int h);                                
static void ASTSetupForSolidFill(ScrnInfoPtr pScrn,
                                 int color, int rop, unsigned int planemask);
static void ASTSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                                       int dst_x, int dst_y, int width, int height);  
static void ASTSetupForSolidLine(ScrnInfoPtr pScrn,
                                 int color, int rop, unsigned int planemask);
static void ASTSubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                          int x, int y, int len, int dir);
static void ASTSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                                           int x1, int y1, int x2, int y2, int flags);                                       
static void ASTSetupForDashedLine(ScrnInfoPtr pScrn,
                                  int fg, int bg, int rop, unsigned int planemask,
                                  int length, UCHAR *pattern);
static void ASTSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                            int x1, int y1, int x2, int y2,
                                            int flags, int phase);                                              
static void ASTSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                                       int patx, int paty, int fg, int bg,
                                       int rop, unsigned int planemask);
static void ASTSubsequentMonoPatternFill(ScrnInfoPtr pScrn,
                                         int patx, int paty,
                                         int x, int y, int w, int h);
static void ASTSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, int patx, int paty,
			                   int rop, unsigned int planemask, int trans_col);
static void ASTSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patx, int paty,
                                                 int x, int y, int w, int h);
static void ASTSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                  int fg, int bg,
                                                  int rop, unsigned int planemask);
static void ASTSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                    int x, int y,
                                                    int width, int height, int skipleft);
static void ASTSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                     int fg, int bg,
                                                     int rop, unsigned int planemask);
static void ASTSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                       int x, int y, int width, int height,
                                                       int src_x, int src_y, int offset);
static void ASTSetClippingRectangle(ScrnInfoPtr pScrn,
                                    int left, int top, int right, int bottom);
static void ASTDisableClipping(ScrnInfoPtr pScrn); 
static void ASTSetHWClipping(ScrnInfoPtr pScrn, int delta_y);

Bool
ASTAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr  infoPtr;
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    ASTRecPtr      pAST = ASTPTR(pScrn);

    pAST->AccelInfoPtr = infoPtr = XAACreateInfoRec();
    if (!infoPtr)  return FALSE;

    infoPtr->Flags = LINEAR_FRAMEBUFFER |
  		     OFFSCREEN_PIXMAPS |
  		     PIXMAP_CACHE;

    /* Sync */
    if (pAST->ENGCaps & ENG_CAP_Sync)
        infoPtr->Sync = ASTSync;

    /* Screen To Screen copy */
    if (pAST->ENGCaps & ENG_CAP_ScreenToScreenCopy)
    {    
        infoPtr->SetupForScreenToScreenCopy =  ASTSetupForScreenToScreenCopy;
        infoPtr->SubsequentScreenToScreenCopy = ASTSubsequentScreenToScreenCopy;
        infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY | NO_PLANEMASK;
    }

    /* Solid fill */
    if (pAST->ENGCaps & ENG_CAP_SolidFill)
    {    
        infoPtr->SetupForSolidFill = ASTSetupForSolidFill;
        infoPtr->SubsequentSolidFillRect = ASTSubsequentSolidFillRect;
        infoPtr->SolidFillFlags = NO_PLANEMASK;
    } 
        
    /* Solid Lines */
    if (pAST->ENGCaps & ENG_CAP_SolidLine)
    {    
        infoPtr->SetupForSolidLine = ASTSetupForSolidLine;
        infoPtr->SubsequentSolidHorVertLine = ASTSubsequentSolidHorVertLine;    
        infoPtr->SubsequentSolidTwoPointLine = ASTSubsequentSolidTwoPointLine;
        infoPtr->SolidLineFlags = NO_PLANEMASK;
    }

    /* Dashed Lines */
    if (pAST->ENGCaps & ENG_CAP_DashedLine)
    {        
        infoPtr->SetupForDashedLine = ASTSetupForDashedLine;
        infoPtr->SubsequentDashedTwoPointLine = ASTSubsequentDashedTwoPointLine;
        infoPtr->DashPatternMaxLength = 64;
        infoPtr->DashedLineFlags = NO_PLANEMASK |
			           LINE_PATTERN_MSBFIRST_LSBJUSTIFIED;
    }			           			           

    /* 8x8 mono pattern fill */
    if (pAST->ENGCaps & ENG_CAP_Mono8x8PatternFill)
    {    
        infoPtr->SetupForMono8x8PatternFill = ASTSetupForMonoPatternFill;
        infoPtr->SubsequentMono8x8PatternFillRect = ASTSubsequentMonoPatternFill;
        infoPtr->Mono8x8PatternFillFlags = NO_PLANEMASK |
                                           NO_TRANSPARENCY |
				           HARDWARE_PATTERN_SCREEN_ORIGIN |
				           HARDWARE_PATTERN_PROGRAMMED_BITS |
				           BIT_ORDER_IN_BYTE_MSBFIRST;
    }				           
				           
    /* 8x8 color pattern fill */
    if (pAST->ENGCaps & ENG_CAP_Color8x8PatternFill)
    {    
        infoPtr->SetupForColor8x8PatternFill = ASTSetupForColor8x8PatternFill;
        infoPtr->SubsequentColor8x8PatternFillRect = ASTSubsequentColor8x8PatternFillRect;
        infoPtr->Color8x8PatternFillFlags = NO_PLANEMASK |
					    NO_TRANSPARENCY |    
	 				    HARDWARE_PATTERN_SCREEN_ORIGIN;
    }	 				    
	 				
    /* CPU To Screen Color Expand */
    if (pAST->ENGCaps & ENG_CAP_CPUToScreenColorExpand)
    {    
        infoPtr->SetupForCPUToScreenColorExpandFill = ASTSetupForCPUToScreenColorExpandFill;
        infoPtr->SubsequentCPUToScreenColorExpandFill = ASTSubsequentCPUToScreenColorExpandFill;
        infoPtr->ColorExpandRange = MAX_PATReg_Size;
        infoPtr->ColorExpandBase = MMIOREG_PAT;
        infoPtr->CPUToScreenColorExpandFillFlags = NO_PLANEMASK |
	   				           BIT_ORDER_IN_BYTE_MSBFIRST;
    }	   				           

    /* Screen To Screen Color Expand */
    if (pAST->ENGCaps & ENG_CAP_ScreenToScreenColorExpand)
    {    
        infoPtr->SetupForScreenToScreenColorExpandFill = ASTSetupForScreenToScreenColorExpandFill;
        infoPtr->SubsequentScreenToScreenColorExpandFill = ASTSubsequentScreenToScreenColorExpandFill;
        infoPtr->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK |
	                                              BIT_ORDER_IN_BYTE_MSBFIRST;
    }	                                              
                                              
    /* Clipping */
    if (pAST->ENGCaps & ENG_CAP_Clipping)
    {    
        infoPtr->SetClippingRectangle = ASTSetClippingRectangle;
        infoPtr->DisableClipping = ASTDisableClipping;
        infoPtr->ClippingFlags = HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY 	|
                		 HARDWARE_CLIP_MONO_8x8_FILL		|
                		 HARDWARE_CLIP_COLOR_8x8_FILL	 	|
                		 HARDWARE_CLIP_SOLID_LINE 		| 
                		 HARDWARE_CLIP_DASHED_LINE 		| 
                		 HARDWARE_CLIP_SOLID_LINE; 
    }                		 

    return(XAAInit(pScreen, infoPtr));
    
} /* end of ASTAccelInit */


static void
ASTSync(ScrnInfoPtr pScrn)
{
    ASTRecPtr pAST = ASTPTR(pScrn);

    /* wait engle idle */
    vWaitEngIdle(pScrn, pAST);

} /* end of ASTSync */


static void ASTSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                          int xdir, int ydir, int rop,
                                          unsigned int planemask, int trans_color)
{
	
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG  cmdreg;
    
/*
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForScreenToScreenCopy\n");
*/    
    /* Modify Reg. Value */
    cmdreg = CMD_BITBLT;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAACopyROP[rop] << 8);
    pAST->ulCMDReg = cmdreg;
     
    if (!pAST->MMIO2D)
    {   
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*2);
                
        ASTSetupSRCPitch(pSingleCMD, pAST->VideoModeInfo.ScreenPitch);  
        pSingleCMD++;
        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        
        /* Update Write Pointer */
        mUpdateWritePointer;
        
    }
    else
    {
        /* Write to MMIO */                
        ASTSetupSRCPitch_MMIO(pAST->VideoModeInfo.ScreenPitch);  
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);    
    }
                             
} /* end of ASTSetupForScreenToScreenCopy */

static void
ASTSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, int x2,
                                int y2, int width, int height)
{
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    int src_x, src_y, dst_x, dst_y;
    ULONG srcbase, dstbase, cmdreg;
    int delta_y = 0;
/*
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentScreenToScreenCopy\n");
*/

    if ((width != 0) && (height != 0))
    {    	
        /* Modify Reg. Value */
        cmdreg = pAST->ulCMDReg;
        if (pAST->EnableClip)
            cmdreg |= CMD_ENABLE_CLIP;
        else
            cmdreg &= ~CMD_ENABLE_CLIP;        
        srcbase = dstbase = 0;

        if (x1 < x2)
            cmdreg |= CMD_X_DEC;     	

        if (y1 < y2)
            cmdreg |= CMD_Y_DEC;     	
    
        if ((y1 + height) >= MAX_SRC_Y)
        {       
            srcbase=pAST->VideoModeInfo.ScreenPitch*y1;
            y1 = 0;
        }       
        
        if ((y2 + height) >= pScrn->virtualY) 
        {  
            delta_y = y2;
            dstbase=pAST->VideoModeInfo.ScreenPitch*y2;
            y2 = 0;
        }
              
        if (cmdreg & CMD_X_DEC)
        {
            src_x = x1 + width - 1;
            dst_x = x2 + width - 1;
        }
        else
        {
            src_x = x1;
            dst_x = x2;	
        }
     
        if (cmdreg & CMD_Y_DEC)
        {        	
            src_y = y1 + height - 1;
            dst_y = y2 + height - 1;
        }
        else
        {
            src_y = y1;
            dst_y = y2;	
        }
        
        if (pAST->EnableClip)                    
            ASTSetHWClipping(pScrn, delta_y);
            
        if (!pAST->MMIO2D)        
        {
            /* Write to CMDQ */
            pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*6);
          
            ASTSetupSRCBase(pSingleCMD, srcbase);
            pSingleCMD++;       
            ASTSetupDSTBase(pSingleCMD, dstbase);
            pSingleCMD++;    
            ASTSetupDSTXY(pSingleCMD, dst_x, dst_y);    
            pSingleCMD++;    
            ASTSetupSRCXY(pSingleCMD, src_x, src_y);    
            pSingleCMD++;    
            ASTSetupRECTXY(pSingleCMD, width, height);    
            pSingleCMD++;    
            ASTSetupCMDReg(pSingleCMD, cmdreg);       
                                      
            /* Update Write Pointer */
            mUpdateWritePointer;
        
        }
        else
        {
            ASTSetupSRCBase_MMIO(srcbase);
            ASTSetupDSTBase_MMIO(dstbase);
            ASTSetupDSTXY_MMIO(dst_x, dst_y);    
            ASTSetupSRCXY_MMIO(src_x, src_y);    
            ASTSetupRECTXY_MMIO(width, height);    
            ASTSetupCMDReg_MMIO(cmdreg);       
                	
            vWaitEngIdle(pScrn, pAST);
        }
        
    } /* width & height check */

} /* end of ASTSubsequentScreenToScreenCopy */

static void
ASTSetupForSolidFill(ScrnInfoPtr pScrn,
                     int color, int rop, unsigned int planemask)
{
	
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG cmdreg;

/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForSolidFill\n");
*/            
    /* Modify Reg. Value */
    cmdreg = CMD_BITBLT | CMD_PAT_FGCOLOR;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);
    pAST->ulCMDReg = cmdreg;
            
    if (!pAST->MMIO2D)                    
    {
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*2);

        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        ASTSetupFG(pSingleCMD, color);
        
        /* Update Write Pointer */
        mUpdateWritePointer;
                
    }
    else
    {
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);
        ASTSetupFG_MMIO(color);            	
    }
           
} /* end of ASTSetupForSolidFill */


static void
ASTSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                           int dst_x, int dst_y, int width, int height)
{
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG dstbase, cmdreg; 
    int delta_y = 0;
           
/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentSolidFillRect\n");
*/

    if ((width != 0) && (height != 0))
    {
        /* Modify Reg. Value */
        cmdreg = pAST->ulCMDReg;
        if (pAST->EnableClip)
            cmdreg |= CMD_ENABLE_CLIP;
        else
            cmdreg &= ~CMD_ENABLE_CLIP;            
        dstbase = 0;
        
        if (dst_y >= pScrn->virtualY) 
        {
            delta_y = dst_y;	   
            dstbase=pAST->VideoModeInfo.ScreenPitch*dst_y;
            dst_y=0;
        }

        if (pAST->EnableClip)                    
            ASTSetHWClipping(pScrn, delta_y);
                                  
        if (!pAST->MMIO2D)                    
        {                  
            /* Write to CMDQ */    
            pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*4);
        
            ASTSetupDSTBase(pSingleCMD, dstbase);
            pSingleCMD++;    
            ASTSetupDSTXY(pSingleCMD, dst_x, dst_y);
            pSingleCMD++;    
            ASTSetupRECTXY(pSingleCMD, width, height);
            pSingleCMD++;    
            ASTSetupCMDReg(pSingleCMD, cmdreg);        
          
            /* Update Write Pointer */
            mUpdateWritePointer;
                    
        }
        else
        {                  
            ASTSetupDSTBase_MMIO(dstbase);
            ASTSetupDSTXY_MMIO(dst_x, dst_y);
            ASTSetupRECTXY_MMIO(width, height);
            ASTSetupCMDReg_MMIO(cmdreg);        
        
            vWaitEngIdle(pScrn, pAST);
          
        }
        
    } /* width & height check */


} /* end of ASTSubsequentSolidFillRect */

/* Line */
static void ASTSetupForSolidLine(ScrnInfoPtr pScrn, 
                                 int color, int rop, unsigned int planemask)
{

    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG  cmdreg;
/*
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForSolidLine\n");
*/  
    /* Modify Reg. Value */
    cmdreg = CMD_BITBLT;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);  
    pAST->ulCMDReg = cmdreg;
     
    if (!pAST->MMIO2D)
    {   
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*3);

        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        ASTSetupFG(pSingleCMD, color);  
        pSingleCMD++;
        ASTSetupBG(pSingleCMD, 0);
        
        /* Update Write Pointer */
        mUpdateWritePointer;        
        
    }
    else
    {
        /* Write to MMIO */   
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);                     
        ASTSetupFG_MMIO(color);  
        ASTSetupBG_MMIO(0);            
    }
                                 
} /* end of ASTSetupForSolidLine */


static void ASTSubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                          int x, int y, int len, int dir)
{

    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG dstbase, cmdreg;   
    int width, height;
    int delta_y = 0;
/*                    
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentSolidHorVertLine\n");
*/

    if (len != 0)
    {
        /* Modify Reg. Value */
        cmdreg = (pAST->ulCMDReg & (~CMD_MASK)) | CMD_BITBLT;
        if (pAST->EnableClip)
            cmdreg |= CMD_ENABLE_CLIP;
        else
            cmdreg &= ~CMD_ENABLE_CLIP;            
        dstbase = 0;
        
        if(dir == DEGREES_0) {			/* horizontal */
            width  = len;
            height = 1;	
        } else {					/* vertical */
            width  = 1;
            height = len;	    	
        }
                  
        if ((y + height) >= pScrn->virtualY) 
        { 
            delta_y = y;	  
            dstbase=pAST->VideoModeInfo.ScreenPitch*y;
            y=0;
        }
                  
        if (pAST->EnableClip)                    
            ASTSetHWClipping(pScrn, delta_y);
                                          
        if (!pAST->MMIO2D)                    
        {                  
            /* Write to CMDQ */    
            pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*4);
        
            ASTSetupDSTBase(pSingleCMD, dstbase);
            pSingleCMD++;    
            ASTSetupDSTXY(pSingleCMD, x, y);
            pSingleCMD++;    
            ASTSetupRECTXY(pSingleCMD, width, height);
            pSingleCMD++;    
            ASTSetupCMDReg(pSingleCMD, cmdreg);        
          
            /* Update Write Pointer */
            mUpdateWritePointer;
                   
        }
        else
        {                  
            ASTSetupDSTBase_MMIO(dstbase);
            ASTSetupDSTXY_MMIO(x, y);
            ASTSetupRECTXY_MMIO(width, height);
            ASTSetupCMDReg_MMIO(cmdreg);        
        
            vWaitEngIdle(pScrn, pAST);
          
        }
        
    } /* len check */
            
} /* end of ASTSubsequentSolidHorVertLine */

static void ASTSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                                           int x1, int y1, int x2, int y2, int flags)
{
 
    ASTRecPtr 	pAST = ASTPTR(pScrn);
    PKT_SC 	*pSingleCMD;
    LINEPARAM   dsLineParam;
    _LINEInfo   LineInfo; 
    ULONG 	dstbase, ulCommand;
    ULONG	miny, maxy;         
    USHORT      usXM;
    int delta_y = 0;
    
/*
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentSolidTwoPointLine\n");
*/    

    /* Modify Reg. Value */
    ulCommand = (pAST->ulCMDReg & (~CMD_MASK)) | CMD_LINEDRAW;
    if(flags & OMIT_LAST)
        ulCommand |= CMD_NOT_DRAW_LAST_PIXEL;
    else
        ulCommand &= ~CMD_NOT_DRAW_LAST_PIXEL;
    if (pAST->EnableClip)
        ulCommand |= CMD_ENABLE_CLIP;
    else
        ulCommand &= ~CMD_ENABLE_CLIP;        
    dstbase = 0;
    miny = (y1 > y2) ? y2 : y1;
    maxy = (y1 > y2) ? y1 : y2;
    if(maxy >= pScrn->virtualY) {
    	delta_y = miny;
        dstbase = pAST->VideoModeInfo.ScreenPitch * miny;
        y1 -= miny;
        y2 -= miny;
    }
       
    LineInfo.X1 = x1;
    LineInfo.Y1 = y1;
    LineInfo.X2 = x2;
    LineInfo.Y2 = y2;
          
    bGetLineTerm(&LineInfo, &dsLineParam);		/* Get Line Parameter */
    
    if (dsLineParam.dwLineAttributes & LINEPARAM_X_DEC) 
        ulCommand |= CMD_X_DEC; 
    if (dsLineParam.dwLineAttributes & LINEPARAM_Y_DEC) 
        ulCommand |= CMD_Y_DEC;             
        
    usXM = (dsLineParam.dwLineAttributes & LINEPARAM_XM) ? 1:0;    

    if (pAST->EnableClip)                    
        ASTSetHWClipping(pScrn, delta_y);
       
    if (!pAST->MMIO2D)                    
    {                  
        /* Write to CMDQ */    
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*7);

        ASTSetupDSTBase(pSingleCMD, dstbase);
        pSingleCMD++;    
        ASTSetupLineXY(pSingleCMD, dsLineParam.dsLineX, dsLineParam.dsLineY);
        pSingleCMD++;    
        ASTSetupLineXMErrTerm(pSingleCMD, usXM , dsLineParam.dwErrorTerm);
        pSingleCMD++; 
        ASTSetupLineWidth(pSingleCMD, dsLineParam.dsLineWidth);
        pSingleCMD++; 
        ASTSetupLineK1Term(pSingleCMD, dsLineParam.dwK1Term);
        pSingleCMD++; 
        ASTSetupLineK2Term(pSingleCMD, dsLineParam.dwK2Term);
        pSingleCMD++;                     
        ASTSetupCMDReg(pSingleCMD, ulCommand);        
              
        /* Update Write Pointer */
        mUpdateWritePointer;                

        /* Patch KDE pass abnormal point, ycchen@052507 */
        vWaitEngIdle(pScrn, pAST);
        
    }
    else
    {                  
        ASTSetupDSTBase_MMIO(dstbase);
        ASTSetupLineXY_MMIO(dsLineParam.dsLineX, dsLineParam.dsLineY);
        ASTSetupLineXMErrTerm_MMIO( usXM , dsLineParam.dwErrorTerm);
        ASTSetupLineWidth_MMIO(dsLineParam.dsLineWidth);
        ASTSetupLineK1Term_MMIO(dsLineParam.dwK1Term);
        ASTSetupLineK2Term_MMIO(dsLineParam.dwK2Term);
        ASTSetupCMDReg_MMIO(ulCommand);        
             
        vWaitEngIdle(pScrn, pAST);
      
    }

                
} /* end of ASTSubsequentSolidTwoPointLine */

/* Dash Line */
static void
ASTSetupForDashedLine(ScrnInfoPtr pScrn,
                      int fg, int bg, int rop, unsigned int planemask,
                      int length, UCHAR *pattern)
{

    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG  cmdreg;
/*
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForDashedLine\n");
*/  
    /* Modify Reg. Value */
    cmdreg = CMD_LINEDRAW | CMD_RESET_STYLE_COUNTER | CMD_ENABLE_LINE_STYLE;
    
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);  
    if(bg == -1) {
        cmdreg |= CMD_TRANSPARENT;    
        bg = 0;
    }
    cmdreg |= (((length-1) & 0x3F) << 24);		/* line period */
    pAST->ulCMDReg = cmdreg;
     
    if (!pAST->MMIO2D)
    {   
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*5);

        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        ASTSetupFG(pSingleCMD, fg);  
        pSingleCMD++;
        ASTSetupBG(pSingleCMD, bg); 
        pSingleCMD++;
        ASTSetupLineStyle1(pSingleCMD, *pattern);
        pSingleCMD++;
        ASTSetupLineStyle2(pSingleCMD, *(pattern+4));
        
        /* Update Write Pointer */
        mUpdateWritePointer;                           
                       
    }
    else
    {
        /* Write to MMIO */   
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);
        ASTSetupFG_MMIO(fg);  
        ASTSetupBG_MMIO(bg); 
        ASTSetupLineStyle1_MMIO(*pattern);
        ASTSetupLineStyle2_MMIO(*(pattern+4));                

    }
                                 	
}

static void
ASTSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int flags, int phase)
{
 
    ASTRecPtr 	pAST = ASTPTR(pScrn);
    PKT_SC 	*pSingleCMD;
    LINEPARAM   dsLineParam;
    _LINEInfo   LineInfo; 
    ULONG 	dstbase, ulCommand; 
    ULONG	miny, maxy;  
    USHORT      usXM;
    int delta_y = 0;
    
/*
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentDashedTwoPointLine\n");
*/   

    /* Modify Reg. Value */
    ulCommand = pAST->ulCMDReg;
    if(flags & OMIT_LAST)
        ulCommand |= CMD_NOT_DRAW_LAST_PIXEL;
    else
        ulCommand &= ~CMD_NOT_DRAW_LAST_PIXEL;
    if (pAST->EnableClip)
        ulCommand |= CMD_ENABLE_CLIP;
    else
        ulCommand &= ~CMD_ENABLE_CLIP;        
    dstbase = 0;        
    miny = (y1 > y2) ? y2 : y1;
    maxy = (y1 > y2) ? y1 : y2;
    if(maxy >= pScrn->virtualY) {
    	delta_y = miny;
        dstbase = pAST->VideoModeInfo.ScreenPitch * miny;
        y1 -= miny;
        y2 -= miny;
    }
   
    LineInfo.X1 = x1;
    LineInfo.Y1 = y1;
    LineInfo.X2 = x2;
    LineInfo.Y2 = y2;
               
    bGetLineTerm(&LineInfo, &dsLineParam);		/* Get Line Parameter */
    
    if (dsLineParam.dwLineAttributes & LINEPARAM_X_DEC) 
        ulCommand |= CMD_X_DEC; 
    if (dsLineParam.dwLineAttributes & LINEPARAM_Y_DEC) 
        ulCommand |= CMD_Y_DEC;             
        
    usXM = (dsLineParam.dwLineAttributes & LINEPARAM_XM) ? 1:0;    

    if (pAST->EnableClip)                    
        ASTSetHWClipping(pScrn, delta_y);
       
    if (!pAST->MMIO2D)                    
    {                  
        /* Write to CMDQ */    
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*7);

        ASTSetupDSTBase(pSingleCMD, dstbase);
        pSingleCMD++;    
        ASTSetupLineXY(pSingleCMD, dsLineParam.dsLineX, dsLineParam.dsLineY);
        pSingleCMD++;    
        ASTSetupLineXMErrTerm(pSingleCMD, usXM , dsLineParam.dwErrorTerm);
        pSingleCMD++; 
        ASTSetupLineWidth(pSingleCMD, dsLineParam.dsLineWidth);
        pSingleCMD++; 
        ASTSetupLineK1Term(pSingleCMD, dsLineParam.dwK1Term);
        pSingleCMD++; 
        ASTSetupLineK2Term(pSingleCMD, dsLineParam.dwK2Term);
        pSingleCMD++;                     
        ASTSetupCMDReg(pSingleCMD, ulCommand);         
              
        /* Update Write Pointer */
        mUpdateWritePointer;

        /* Patch KDE pass abnormal point, ycchen@052507 */
        vWaitEngIdle(pScrn, pAST);
              
    }
    else
    {                  
        ASTSetupDSTBase_MMIO(dstbase);
        ASTSetupLineXY_MMIO(dsLineParam.dsLineX, dsLineParam.dsLineY);
        ASTSetupLineXMErrTerm_MMIO( usXM , dsLineParam.dwErrorTerm);
        ASTSetupLineWidth_MMIO(dsLineParam.dsLineWidth);
        ASTSetupLineK1Term_MMIO(dsLineParam.dwK1Term);
        ASTSetupLineK2Term_MMIO(dsLineParam.dwK2Term);
        ASTSetupCMDReg_MMIO(ulCommand);        
       
        vWaitEngIdle(pScrn, pAST);
      
    }
                	
}

/* Mono Pattern Fill */
static void
ASTSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                           int patx, int paty, int fg, int bg,
                           int rop, unsigned int planemask)
{
	
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG cmdreg;

/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForMonoPatternFill\n");
*/            
    /* Modify Reg. Value */
    cmdreg = CMD_BITBLT | CMD_PAT_MONOMASK;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);
    pAST->ulCMDReg = cmdreg;
            
    if (!pAST->MMIO2D)                    
    {
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*5);

        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        ASTSetupFG(pSingleCMD, fg);  
        pSingleCMD++;
        ASTSetupBG(pSingleCMD, bg);
        pSingleCMD++;
        ASTSetupMONO1(pSingleCMD, patx);  
        pSingleCMD++;
        ASTSetupMONO2(pSingleCMD, paty);
        
        /* Update Write Pointer */
        mUpdateWritePointer;
                                           
    }
    else
    {
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);
        ASTSetupFG_MMIO(fg);   
        ASTSetupBG_MMIO(bg);
        ASTSetupMONO1_MMIO(patx);  
        ASTSetupMONO2_MMIO(paty);                     	
    }
           	
} /* end of ASTSetupForMonoPatternFill */

                      
static void
ASTSubsequentMonoPatternFill(ScrnInfoPtr pScrn,
                             int patx, int paty,
                             int dst_x, int dst_y, int width, int height)
{
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG dstbase, cmdreg;
    int delta_y = 0;
            
/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentMonoPatternFill\n");
*/    
            
    /* Modify Reg. Value */
    cmdreg = pAST->ulCMDReg;
    if (pAST->EnableClip)
        cmdreg |= CMD_ENABLE_CLIP;
    else
        cmdreg &= ~CMD_ENABLE_CLIP;            
    dstbase = 0;

    if (dst_y >= pScrn->virtualY) 
    {   
    	delta_y = dst_y;
        dstbase=pAST->VideoModeInfo.ScreenPitch*dst_y;
        dst_y=0;
    }

    if (pAST->EnableClip)                    
        ASTSetHWClipping(pScrn, delta_y);
                         
    if (!pAST->MMIO2D)                    
    {                  
        /* Write to CMDQ */    
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*4);

        ASTSetupDSTBase(pSingleCMD, dstbase);
        pSingleCMD++;    
        ASTSetupDSTXY(pSingleCMD, dst_x, dst_y);
        pSingleCMD++;    
        ASTSetupRECTXY(pSingleCMD, width, height);
        pSingleCMD++;    
        ASTSetupCMDReg(pSingleCMD, cmdreg);        
      
        /* Update Write Pointer */
        mUpdateWritePointer;
              
    }
    else
    {                  
        ASTSetupDSTBase_MMIO(dstbase);
        ASTSetupDSTXY_MMIO(dst_x, dst_y);
        ASTSetupRECTXY_MMIO(width, height);
        ASTSetupCMDReg_MMIO(cmdreg);        
 
        vWaitEngIdle(pScrn, pAST);      
    }	

} /* end of ASTSubsequentMonoPatternFill */

static void
ASTSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, int patx, int paty,
			       int rop, unsigned int planemask, int trans_col)
{
	
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG cmdreg;
    CARD32 *pataddr;
    ULONG ulPatSize;
    int i, j, cpp;
/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForColor8x8PatternFill\n");
*/            
    /* Modify Reg. Value */
    cmdreg = CMD_BITBLT | CMD_PAT_PATREG;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);
    pAST->ulCMDReg = cmdreg;
    cpp = (pScrn->bitsPerPixel + 1) / 8;
    pataddr = (CARD32 *)(pAST->FBVirtualAddr +
                        (paty * pAST->VideoModeInfo.ScreenWidth) + (patx * cpp));   
    ulPatSize = 8*8*cpp;
                
    if (!pAST->MMIO2D)                    
    {
        /* Write to CMDQ */        
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*(1 + ulPatSize/4));
        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        for (i=0; i<8; i++)
        {
            for (j=0; j<8*cpp/4; j++)
            {
                ASTSetupPatReg(pSingleCMD, (i*j + j) , (*(CARD32 *) (pataddr++)));
                pSingleCMD++;                	
            }	
        }
        
        /* Update Write Pointer */
        mUpdateWritePointer;
                        
    }
    else
    {    	
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);
        for (i=0; i<8; i++)
        {
            for (j=0; j<8*cpp/4; j++)
            {
                ASTSetupPatReg_MMIO((i*j + j) , (*(CARD32 *) (pataddr++)));
            }	
        }                
             	
    }

} /* end of ASTSetupForColor8x8PatternFill */
		       
static void
ASTSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patx, int paty,
                                     int dst_x, int dst_y, int width, int height)
{
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG dstbase, cmdreg;
    int delta_y = 0;
            
/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentColor8x8PatternFillRect\n");
*/
            
    /* Modify Reg. Value */
    cmdreg = pAST->ulCMDReg;
    if (pAST->EnableClip)
        cmdreg |= CMD_ENABLE_CLIP;
    else
        cmdreg &= ~CMD_ENABLE_CLIP;            
    dstbase = 0;

    if (dst_y >= pScrn->virtualY) 
    {   
    	delta_y = dst_y;
        dstbase=pAST->VideoModeInfo.ScreenPitch*dst_y;
        dst_y=0;
    }

    if (pAST->EnableClip)                    
        ASTSetHWClipping(pScrn, delta_y);
                  
    if (!pAST->MMIO2D)                    
    {                  
        /* Write to CMDQ */    
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*4);

        ASTSetupDSTBase(pSingleCMD, dstbase);
        pSingleCMD++;    
        ASTSetupDSTXY(pSingleCMD, dst_x, dst_y);
        pSingleCMD++;    
        ASTSetupRECTXY(pSingleCMD, width, height);
        pSingleCMD++;    
        ASTSetupCMDReg(pSingleCMD, cmdreg);        
      
        /* Update Write Pointer */
        mUpdateWritePointer;
                
    }
    else
    {                  
        ASTSetupDSTBase_MMIO(dstbase);
        ASTSetupDSTXY_MMIO(dst_x, dst_y);
        ASTSetupRECTXY_MMIO(width, height);
        ASTSetupCMDReg_MMIO(cmdreg);        
 
        vWaitEngIdle(pScrn, pAST);      
    }	
	
} /* ASTSubsequentColor8x8PatternFillRect */

/* CPU to Screen Expand */
static void
ASTSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                      int fg, int bg,
                                      int rop, unsigned int planemask)
{

    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG cmdreg;

/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForCPUToScreenColorExpandFill\n");
*/           
    /* Modify Reg. Value */
    cmdreg = CMD_COLOREXP;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);
    if(bg == -1) {
        cmdreg |= CMD_FONT_TRANSPARENT;    
        bg = 0;
    }    
    pAST->ulCMDReg = cmdreg;
            
    if (!pAST->MMIO2D)                    
    {
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*3);

        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        ASTSetupFG(pSingleCMD, fg);  
        pSingleCMD++;
        ASTSetupBG(pSingleCMD, bg); 

        /* Update Write Pointer */
        mUpdateWritePointer;
       
    }
    else
    {
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);
        ASTSetupFG_MMIO(fg); 
        ASTSetupBG_MMIO(bg); 
                   	
    }
           	
}
                               
static void
ASTSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                        int dst_x, int dst_y,
                                        int width, int height, int offset)
{

    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG dstbase, cmdreg;
    int delta_y = 0;

/*           
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentCPUToScreenColorExpandFill\n");
*/
            
    /* Modify Reg. Value */
    cmdreg = pAST->ulCMDReg;
    if (pAST->EnableClip)
        cmdreg |= CMD_ENABLE_CLIP;
    else
        cmdreg &= ~CMD_ENABLE_CLIP;            
    dstbase = 0;

    if (dst_y >= pScrn->virtualY) 
    {   
    	delta_y = dst_y;
        dstbase=pAST->VideoModeInfo.ScreenPitch*dst_y;
        dst_y=0;
    }

    if (pAST->EnableClip)                    
        ASTSetHWClipping(pScrn, delta_y);
                                    
    if (!pAST->MMIO2D)                    
    {                  
        /* Write to CMDQ */    
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*5);

        ASTSetupSRCPitch(pSingleCMD, ((width+7)/8));  
        pSingleCMD++;    
        ASTSetupDSTBase(pSingleCMD, dstbase);
        pSingleCMD++;    
        ASTSetupDSTXY(pSingleCMD, dst_x, dst_y);
        pSingleCMD++;    
        ASTSetupRECTXY(pSingleCMD, width, height);
        pSingleCMD++;    
        ASTSetupCMDReg(pSingleCMD, cmdreg);        
      
        /* Update Write Pointer */
        mUpdateWritePointer;
               
    }
    else
    {  
        ASTSetupSRCPitch_MMIO((width+7)/8);      	                
        ASTSetupDSTBase_MMIO(dstbase);
        ASTSetupDSTXY_MMIO(dst_x, dst_y);
        ASTSetupSRCXY_MMIO(0, 0);
        
        ASTSetupRECTXY_MMIO(width, height);
        ASTSetupCMDReg_MMIO(cmdreg);        
 
        vWaitEngIdle(pScrn, pAST);
      
    }
    	
}


/* Screen to Screen Color Expand */
static void
ASTSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                         int fg, int bg,
                                         int rop, unsigned int planemask)
{

    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG cmdreg;

/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetupForScreenToScreenColorExpandFill\n");
*/
           
    /* Modify Reg. Value */
    cmdreg = CMD_ENHCOLOREXP;
    switch (pAST->VideoModeInfo.bitsPerPixel)
    {
    case 8:
        cmdreg |= CMD_COLOR_08;
        break;
    case 15:
    case 16:
        cmdreg |= CMD_COLOR_16;
        break;    
    case 24:
    case 32:
        cmdreg |= CMD_COLOR_32;
        break;    	
    }
    cmdreg |= (ASTXAAPatternROP[rop] << 8);
    if(bg == -1) {
        cmdreg |= CMD_FONT_TRANSPARENT;    
        bg = 0;
    }    
    pAST->ulCMDReg = cmdreg;
            
    if (!pAST->MMIO2D)                    
    {
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*3);

        ASTSetupDSTPitchHeight(pSingleCMD, pAST->VideoModeInfo.ScreenPitch, -1);
        pSingleCMD++;
        ASTSetupFG(pSingleCMD, fg);  
        pSingleCMD++;
        ASTSetupBG(pSingleCMD, bg); 

        /* Update Write Pointer */
        mUpdateWritePointer;
       
    }
    else
    {
        ASTSetupDSTPitchHeight_MMIO(pAST->VideoModeInfo.ScreenPitch, -1);
        ASTSetupFG_MMIO(fg); 
        ASTSetupBG_MMIO(bg); 
                   	
    }
           	
}



static void
ASTSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                           int dst_x, int dst_y, int width, int height,
                                           int src_x, int src_y, int offset)
{
   ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
    ULONG srcbase, dstbase, cmdreg;
    USHORT srcpitch;
    int delta_y = 0;

/*           
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSubsequentScreenToScreenColorExpandFill\n");
*/
            
    /* Modify Reg. Value */
    cmdreg = pAST->ulCMDReg;
    if (pAST->EnableClip)
        cmdreg |= CMD_ENABLE_CLIP;
    else
        cmdreg &= ~CMD_ENABLE_CLIP;            
    dstbase = 0;
    if (dst_y >= pScrn->virtualY) 
    {   
    	delta_y = dst_y;
        dstbase=pAST->VideoModeInfo.ScreenPitch*dst_y;
        dst_y=0;
    }
    srcbase = pAST->VideoModeInfo.ScreenPitch*src_y + ((pScrn->bitsPerPixel+1)/8)*src_x;            
    srcpitch = (pScrn->displayWidth+7)/8;

    if (pAST->EnableClip)                    
        ASTSetHWClipping(pScrn, delta_y);
    
    if (!pAST->MMIO2D)                    
    {                  
        /* Write to CMDQ */    
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*6);

        ASTSetupSRCBase(pSingleCMD, srcbase);
        pSingleCMD++;  
        ASTSetupSRCPitch(pSingleCMD,srcpitch);  
        pSingleCMD++;    
        ASTSetupDSTBase(pSingleCMD, dstbase);
        pSingleCMD++;    
        ASTSetupDSTXY(pSingleCMD, dst_x, dst_y);
        pSingleCMD++;    
        ASTSetupRECTXY(pSingleCMD, width, height);
        pSingleCMD++;    
        ASTSetupCMDReg(pSingleCMD, cmdreg);        
      
        /* Update Write Pointer */
        mUpdateWritePointer;
       
    }
    else
    { 
        ASTSetupSRCBase_MMIO(srcbase);    	 
        ASTSetupSRCPitch_MMIO(srcpitch);      	                
        ASTSetupDSTBase_MMIO(dstbase);
        ASTSetupDSTXY_MMIO(dst_x, dst_y);
        ASTSetupRECTXY_MMIO(width, height);
        ASTSetupCMDReg_MMIO(cmdreg);        
 
        vWaitEngIdle(pScrn, pAST);
      
    }
		
}

	  
/* Clipping */
static void
ASTSetHWClipping(ScrnInfoPtr pScrn, int delta_y)
{
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;

    if (!pAST->MMIO2D)                    
    {
        /* Write to CMDQ */
        pSingleCMD = (PKT_SC *) pjRequestCMDQ(pAST, PKT_SINGLE_LENGTH*2);

        ASTSetupCLIP1(pSingleCMD, pAST->clip_left, pAST->clip_top - delta_y);
        pSingleCMD++;
        ASTSetupCLIP2(pSingleCMD, pAST->clip_right + 1, pAST->clip_bottom - delta_y + 1);
        
        /* Update Write Pointer */
        mUpdateWritePointer;
                         
    }
    else
    {
        ASTSetupCLIP1_MMIO(pAST->clip_left, pAST->clip_top - delta_y);
        ASTSetupCLIP2_MMIO(pAST->clip_right + 1, pAST->clip_bottom - delta_y + 1);                   	
    }
	
}	

static void
ASTSetClippingRectangle(ScrnInfoPtr pScrn,
                        int left, int top, int right, int bottom)
{
	
    ASTRecPtr pAST = ASTPTR(pScrn);
    PKT_SC *pSingleCMD;
/*            
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTSetClippingRectangle\n");
*/            
    pAST->EnableClip = TRUE;
            
    pAST->clip_left   = left;
    pAST->clip_top    = top;
    pAST->clip_right  = right;
    pAST->clip_bottom = bottom;
    
}

static void
ASTDisableClipping(ScrnInfoPtr pScrn)
{
    ASTRecPtr pAST = ASTPTR(pScrn);
/*    
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ASTDisableClipping\n");
*/    
    pAST->EnableClip = FALSE;
}

#endif	/* end of Accel_2D */
