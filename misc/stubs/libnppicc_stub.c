#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef enum
{
    NPP_ERROR                               = -2,
    NPP_ERROR_RESERVED                      = -1,
    NPP_NO_ERROR                            = 0, 
    NPP_SUCCESS = NPP_NO_ERROR   
} NppStatus;

typedef unsigned char       Npp8u;     /**<  8-bit unsigned chars */
typedef signed char         Npp8s;     /**<  8-bit signed chars */
typedef unsigned short      Npp16u;    /**<  16-bit unsigned integers */
typedef short               Npp16s;    /**<  16-bit signed integers */
typedef unsigned int        Npp32u;    /**<  32-bit unsigned integers */
typedef int                 Npp32s;    /**<  32-bit signed integers */
typedef unsigned long long  Npp64u;    /**<  64-bit unsigned integers */
typedef long long           Npp64s;    /**<  64-bit signed integers */
typedef float               Npp32f;    /**<  32-bit (IEEE) floating-point numbers */
typedef double              Npp64f;    /**<  64-bit floating-point numbers */

/**
 * 2D Size
 * This struct typically represents the size of a a rectangular region in
 * two space.
 */
typedef struct
{
    int width;  /**<  Rectangle width. */
    int height; /**<  Rectangle height. */
} NppiSize;

NppStatus nppiYCbCr420_8u_P2P3R(const Npp8u * const pSrcY, int nSrcYStep, const Npp8u * pSrcCbCr, int nSrcCbCrStep, Npp8u * pDst[3], int rDstStep[3], NppiSize oSizeROI)
{
    return NPP_ERROR;
}

NppStatus nppiYCbCr420_8u_P3P2R(const Npp8u * const pSrc[3], int rSrcStep[3], Npp8u * pDstY, int nDstYStep, Npp8u * pDstCbCr, int nDstCbCrStep, NppiSize oSizeROI)
{
    return NPP_ERROR;
}
