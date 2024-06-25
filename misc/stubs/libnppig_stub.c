#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
	NPP_ERROR = -2,
	NPP_ERROR_RESERVED = -1,
	NPP_NO_ERROR = 0,
	NPP_SUCCESS = NPP_NO_ERROR
} NppStatus;

typedef unsigned char Npp8u;	   /**<  8-bit unsigned chars */
typedef signed char Npp8s;		   /**<  8-bit signed chars */
typedef unsigned short Npp16u;	   /**<  16-bit unsigned integers */
typedef short Npp16s;			   /**<  16-bit signed integers */
typedef unsigned int Npp32u;	   /**<  32-bit unsigned integers */
typedef int Npp32s;				   /**<  32-bit signed integers */
typedef unsigned long long Npp64u; /**<  64-bit unsigned integers */
typedef long long Npp64s;		   /**<  64-bit signed integers */
typedef float Npp32f;			   /**<  32-bit (IEEE) floating-point numbers */
typedef double Npp64f;			   /**<  64-bit floating-point numbers */

/**
 * 2D Size
 * This struct typically represents the size of a a rectangular region in
 * two space.
 */
typedef struct {
	int width;	/**<  Rectangle width. */
	int height; /**<  Rectangle height. */
} NppiSize;

/**
 * 2D Rectangle
 * This struct contains position and size information of a rectangle in
 * two space.
 * The rectangle's position is usually signified by the coordinate of its
 * upper-left corner.
 */
typedef struct {
	int x; /**<  x-coordinate of upper left corner (lowest memory address). */
	int y; /**<  y-coordinate of upper left corner (lowest memory address). */
	int width;	/**<  Rectangle width. */
	int height; /**<  Rectangle height. */
} NppiRect;

NppStatus nppiResizeSqrPixel_8u_C1R(const Npp8u* pSrc, NppiSize oSrcSize,
									int nSrcStep, NppiRect oSrcROI, Npp8u* pDst,
									int nDstStep, NppiRect oDstROI,
									double nXFactor, double nYFactor,
									double nXShift, double nYShift,
									int eInterpolation) 
{
	return NPP_ERROR;
}
