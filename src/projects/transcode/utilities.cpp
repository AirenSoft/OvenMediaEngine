#include "utilities.h"


namespace AVUtilities
{
#if DEBUG_PREVIEW_ENABLE
	namespace FrameConvert
	{
		// 가로 축 기준으로 변환
		bool AVFrameToCvMat(AVFrame* pSrc, Mat** pDst, double asix_width)
		{
		    static struct SwsContext *img_convert_ctx = NULL;
		    static int old_width_in = 0;
		    static int old_width_out = 0;
		    static int old_height_in = 0;
		    static int old_height_out = 0;

		    AVPicture                       pict;

		    int width_in = pSrc->width;
		    int height_in = pSrc->height;
		    int format = pSrc->format;

		    double aspect_ratio = (double)width_in / (double)height_in;
		    // 영상의 사이즈를 25%로 축소함
#if 0		    
		    int width_out = (int)((double)width_in * (scale/100));
		    int height_out = (int)((double)height_in * (scale/100));
#else
		    int width_out = (int)asix_width;
		    if(width_out%2 != 0) width_out += 1;
		    int height_out = (int)((double)width_out / aspect_ratio);
		    if(height_out%2 != 0) height_out += 1;

#endif

		    // HLogI("Create AVPicture");
		    avpicture_alloc(&pict, AV_PIX_FMT_BGR24, width_out, height_out);
		 
		 	if(old_height_in != height_in || old_width_in != width_in || old_width_out != width_out || old_height_out != height_out)
		 	{
		 		 if(img_convert_ctx)
		 		 {
		 		 	sws_freeContext(img_convert_ctx);
		 		 	img_convert_ctx = NULL;
		 		 }
		 	}

		    // BGR24 포맷으로 변경해야함
		    if(!img_convert_ctx)
		    {
		        img_convert_ctx = sws_getContext(
		                width_in,
		                height_in,
		                (AVPixelFormat)format,
		                width_out,
		                height_out,
		                AV_PIX_FMT_BGR24,
		                SWS_FAST_BILINEAR,
		                NULL, NULL, NULL);

		        old_width_in = width_in;
		        old_height_in = height_in;
		        old_width_out = width_out;
		        old_height_out = height_out;
		    }

		    // 변경
		    if(img_convert_ctx)
		    {
		           sws_scale(img_convert_ctx, pSrc->data, pSrc->linesize,0, height_in, pict.data, pict.linesize);
		    }


		    // CV Matric 
		    cv::Mat Converted( height_out, width_out, CV_8UC3, pict.data[0]);
		  
		    *pDst = new Mat();
		    if(*pDst == NULL)
		    {
		            return false;
		    }

		    // 데이터 복사
		    Converted.copyTo(**pDst);

	    	// HLogI("convert complete. to %dx%d", (*pDst)->cols, (*pDst)->rows);
		    
		    // 데이터 삭제
		    avpicture_free(&pict);

		    return true;
		}


	}
#endif
}
