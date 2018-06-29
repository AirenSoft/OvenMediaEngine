#ifndef __COMMON_AVUtilities_INCLUDED__
#define __COMMON_AVUtilities_INCLUDED__

// ## FFMEPG
extern "C" {
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavcodec/avcodec.h>
	#include <libswresample/swresample.h>
	#include <libavutil/avutil.h>
	#include <libavutil/opt.h>
	#include <libavutil/fifo.h>
	#include <libavutil/parseutils.h>
	#include <libavutil/pixdesc.h>
	#include <libavfilter/avfilter.h>
	#include <libavfilter/avfiltergraph.h>
	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libavformat/avio.h>
	#include <libavutil/threadmessage.h>
	#include <libavutil/time.h>
};


// ## OPENCV
#define DEBUG_PREVIEW_ENABLE 	0

#if DEBUG_PREVIEW_ENABLE
	#include <opencv2/opencv.hpp>
	using namespace cv;
#endif 

namespace AVUtilities
{
#if DEBUG_PREVIEW_ENABLE
	namespace FrameConvert
	{
		bool AVFrameToCvMat(AVFrame* pSrc, Mat** pDst, double scale = 100);
	}
#endif
}

#endif
