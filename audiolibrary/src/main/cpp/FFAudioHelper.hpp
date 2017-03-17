//
//  FFAudioHelper.hpp
//  playbook
//
//  Created by fanzhang on 16/8/30.
//  Copyright © 2016年 baobeiyixiaoshi. All rights reserved.
//

#ifndef FFAudioHelper_hpp
#define FFAudioHelper_hpp

extern "C" {
#include <libavutil/avstring.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <iostream>
#include <vector>
#include <memory>

#include "ErrorCheck.h"
#include "FFAutoReleasePool.hpp"

#define AV_ERROR_CHECK(err)												\
do {																	\
    if (err < 0)                                                        \
    {																	\
        std::cerr << "AV_ERROR_CHECK err = " << err << " " << getErrorText(err) << " " << __FILE__ << ":(" << __LINE__ << ")" << std::endl;                   \
        goto Exit0;														\
    }																	\
} while(0)

#define AV_ERROR_CHECKEX(err, exp)										\
do {																	\
    if (err < 0)                                                        \
    {																	\
        std::cerr << "AV_ERROR_CHECK err = " << err << " " << getErrorText(err) << " " << __FILE__ << ":(" << __LINE__ << ")" << std::endl;                   \
        exp;                                                            \
        goto Exit0;														\
    }                                                                   \
} while(0)

namespace
{
    const int outputAudioChannelNum = 1;
    const int outputSampleRate      = 44100;
}

namespace FFAudioHelper
{
    typedef struct AVProcessContext
    {
        AVFormatContext*    format;
        AVCodecContext*     codec;
        AVFilterContext*    filter;
        int                 streamIndex;
        AVFilterContext*    lastFilter;
        int64_t             currentPTS;
        
        FFAutoReleasePool pool;
        
        AVProcessContext();
        AVProcessContext(AVFormatContext* format_, AVCodecContext* codec_, AVFilterContext* filter_, int streamIndex_);
    }
    AVProcessContext;
    
    typedef struct AVCombineQueueContext
    {
        std::vector<std::shared_ptr<AVProcessContext>> inputQueue;
        AVFilterContext*                destFilter;
    }
    AVCombineQueueContext;
    
    int getFileDuration(const std::string& file, int64_t& duration);
    std::string getErrorText(int err);
    int openInputFile(const std::string& inputFile, AVFormatContext*& formatContext, AVCodecContext*& codecContext, int& streamIndex);
    int openOutputFile(const std::string& outputFile,
                              AVFormatContext*& formatContext,
                              AVCodecContext*& codecContext,
                              const std::string& fileType,
                              const int bitrate);
    int configFilterGraphForMixing(const int64_t wholeDuration,
                                           const AVCodecContext* inputCodec1, AVFilterContext*& inputFilter1,
                                           const AVCodecContext* inputCodec2, AVFilterContext*& inputFilter2,
                                           const AVCodecContext* outputCodec, AVFilterContext*& outputFilter,
                                           AVFilterGraph*& graph);
    int decodeOneFrame(AVFormatContext* inputFormat, AVCodecContext* inputCodec, int inputStream, AVFrame* frame, int64_t& globalPTS, bool& finished);
    int tryDecodeOneFrame(AVFormatContext* inputFormat, AVCodecContext* inputCodec, int inputStream, AVFrame* frame, bool& dataPresent, bool& finished);
    int encodeOneFrame(AVFormatContext* outputFormat, AVCodecContext* outputCodec, AVFrame* frame, int64_t& packetPts);
    int encodeFlush(AVFormatContext* outputFormat, AVCodecContext* outputCodec, int64_t& packetPts);
    int processAll(std::vector<AVProcessContext>& inputContexts, const AVProcessContext& outputContext);
    
    int makeInput(AVFilterGraph* graph, const AVCodecContext* codec, AVFilterContext*& input);
    int makeOutput(AVFilterGraph* graph, const AVCodecContext* codec, AVFilterContext* input, AVFilterContext*& output);
    int makeFormatForAMIX(AVFilterGraph* graph, AVFilterContext* input, AVFilterContext*& output);
    int makeFormatForOutput(AVFilterGraph* graph, const AVCodecContext* codec, AVFilterContext* input, AVFilterContext*& output);
    int makePad(AVFilterGraph* graph, AVFilterContext* input, int64_t padDuration, AVFilterContext*& output);
    int makePadWhole(AVFilterGraph* graph, AVFilterContext* input, int64_t wholeDuration, AVFilterContext*& output);
    int makeTrim(AVFilterGraph* graph, AVFilterContext* input, int64_t wholeDuration, AVFilterContext*& output);
    int makeFade(AVFilterGraph* graph, AVFilterContext* input, bool fadeOut, int64_t start, int64_t nb, AVFilterContext*& output);
    int makeDelay(AVFilterGraph* graph, AVFilterContext* input, int64_t delayDuration, AVFilterContext*& output);
    int makeVolume(AVFilterGraph* graph, AVFilterContext* input, double volume, AVFilterContext*& output);
    int makeSplit(AVFilterGraph* graph, AVFilterContext* input, int count, std::vector<AVFilterContext*>& outputs);
    int makeMix(AVFilterGraph* graph, const std::vector<AVFilterContext*>& inputs, AVFilterContext*& output);
    int makeConcat(AVFilterGraph* graph, const std::vector<AVFilterContext*>& inputs, AVFilterContext*& output);
    int makeLoudNorm(AVFilterGraph* graph, AVFilterContext* input, AVFilterContext*& output);
    
    int configInputFilter(AVFilterContext* filter, const AVCodecContext* codec);
    int configInputFilter(AVFilterContext* filter, AVSampleFormat sampleFormat, int sampleRate, int  channels);
    int configInputFilterForAmix(AVFilterContext* filter);
    int configFormatFilter(AVFilterContext* filter, const AVCodecContext* codec);
    int configFormatFilterForAmix(AVFilterContext* filter);
    
    bool packetIsID3(const AVPacket* packet);
}


#endif /* FFAudioHelper_hpp */
