//
//  FFAudioMixing.cpp
//  FFAudioMixing
//
//  Created by fanzhang on 16/3/27.
//  Copyright © 2016年 bbo. All rights reserved.
//



#include <vector>
#include <tuple>
#include <sstream>
#include <cassert>
#include <memory>
#include <algorithm>

#include "FFAudioMixing.hpp"
#include "FFAudioHelper.hpp"

using namespace FFAudioHelper;

namespace
{
    const int64_t MAX_EFFECT_DURATION = (15 * outputSampleRate);
}

namespace
{
    struct FFGlobalInit
    {
        FFGlobalInit()
        {
            av_register_all();
            avcodec_register_all();
            avfilter_register_all();
        }
    }
    _FFGlobalInit;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

class FFAudioMixing : virtual public IFFAudioMixing
{
public:
    std::string _outputFileType;
    int _outputBitRate;
    
public:
    FFAudioMixing()
    : _outputBitRate(0)
    {
        
    }
    
    virtual ~FFAudioMixing()
    {
        
    }
    
    virtual void init(const char* outputFileType, const int outputBitRate)
    {
        _outputFileType = outputFileType;
        _outputBitRate = outputBitRate;
    }
    
    virtual void destroy()
    {
        delete this;
    }

    virtual int mixAudio(const std::string& inputFile1, const std::string inputFile2, const std::string& outputFile)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            // open input file 1
            int64_t duration1 = 0;
            err = getFileDuration(inputFile1, duration1);
            AV_ERROR_CHECK(err);
            
            AVFormatContext* inputFormat1 = NULL;
            pool.autoRelease([&inputFormat1]
                             {
                                 if (inputFormat1)
                                     avformat_close_input(&inputFormat1);
                             });
            
            AVCodecContext* inputCodec1 = NULL;
            pool.autoRelease([&inputCodec1]
                             {
                                 if (inputCodec1)
                                 {
                                     avcodec_close(inputCodec1);
                                     inputCodec1 = NULL;
                                 }
                             });
            
            int inputStream1 = 0;
            err = openInputFile(inputFile1, inputFormat1, inputCodec1, inputStream1);
            AV_ERROR_CHECK(err);
            
            // open input file 2
            int64_t duration2 = 0;
            err = getFileDuration(inputFile2, duration2);
            AV_ERROR_CHECK(err);
            
            AVFormatContext* inputFormat2 = NULL;
            pool.autoRelease([&inputFormat2]
                             {
                                 if (inputFormat2)
                                     avformat_close_input(&inputFormat2);
                             });
            
            AVCodecContext* inputCodec2 = NULL;
            pool.autoRelease([&inputCodec2]
                             {
                                 if (inputCodec2)
                                 {
                                     avcodec_close(inputCodec2);
                                     inputCodec2 = NULL;
                                 }
                             });
            
            int inputStream2 = 0;
            err = openInputFile(inputFile2, inputFormat2, inputCodec2, inputStream2);
            AV_ERROR_CHECK(err);
            
            // open output file
            AVFormatContext* outputFormat = NULL;
            pool.autoRelease([&outputFormat]
                             {
                                 if (outputFormat)
                                 {
                                     if (outputFormat->pb)
                                         avio_closep(&outputFormat->pb);
                                     avformat_free_context(outputFormat);
                                     outputFormat = NULL;
                                 }
                             });
            
            AVCodecContext* outputCodec = NULL;
            pool.autoRelease([&outputCodec]
                             {
                                 if (outputCodec)
                                 {
                                     avcodec_close(outputCodec);
                                     outputCodec = NULL;
                                 }
                             });
            
            err = openOutputFile(outputFile, outputFormat, outputCodec, _outputFileType, _outputBitRate);
            AV_ERROR_CHECK(err);
            
            // init filter
            AVFilterContext* inputFilter1 = NULL;
            AVFilterContext* inputFilter2 = NULL;
            AVFilterContext* outputFilter = NULL;
            AVFilterGraph* graph = NULL;
            pool.autoRelease([&graph]
                             {
                                 if (graph)
                                     avfilter_graph_free(&graph);
                             });
            
            int64_t wholeDuration = std::max(duration1, duration2) + 2 * outputSampleRate;
            err = configFilterGraphForMixing(wholeDuration, inputCodec1, inputFilter1, inputCodec2, inputFilter2, outputCodec, outputFilter, graph);
            AV_ERROR_CHECK(err);
            
            // write output file header
            err = avformat_write_header(outputFormat, NULL);
            AV_ERROR_CHECK(err);
            
            // process all data
            std::vector<AVProcessContext> inputContexts;
            inputContexts.push_back(AVProcessContext(inputFormat1, inputCodec1, inputFilter1, inputStream1));
            inputContexts.push_back(AVProcessContext(inputFormat2, inputCodec2, inputFilter2, inputStream2));
            
            err = processAll(inputContexts, AVProcessContext(outputFormat, outputCodec, outputFilter, 0));
            AV_ERROR_CHECK(err);
            
            // write trailer
            err = av_write_trailer(outputFormat);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    virtual int combineAudios(const std::string&                beginEffect,
                              const std::string&                endEffect,
                              bool                              haveIntroPage,
                              bool                              haveEndingPage,
                              const std::vector<std::string>&   voicePages,
                              double                            timeSpanSec,
                              const std::string&                bkgMusicFile,
                              double                            bkgVolume,
                              const std::string&                outputFile)
    
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            // calculate background time range
            int64_t timeSpan = timeSpanSec * outputSampleRate;
            int64_t wholeDuration = 0;
            int64_t backgroundDelayStart = 0;
            int64_t backgroundPadEnd = 0;
            
            std::vector<std::string> foregroundPages;
            
            if (beginEffect.length())
            {
                foregroundPages.push_back(beginEffect);
                
                int64_t duration = 0;
                err = getFileDuration(beginEffect, duration);
                AV_ERROR_CHECK(err);
                duration = std::min(duration, MAX_EFFECT_DURATION);
                
                backgroundDelayStart += duration;
                backgroundDelayStart += timeSpan;
            }
            
            if (haveIntroPage)
            {
                std::string introPage = *voicePages.begin();
                int64_t duration = 0;
                err = getFileDuration(introPage, duration);
                AV_ERROR_CHECK(err);
                backgroundDelayStart += duration;
                backgroundDelayStart += timeSpan;
            }
            
            if (haveEndingPage)
            {
                std::string endingPage = *voicePages.rbegin();
                int64_t duration = 0;
                err = getFileDuration(endingPage, duration);
                AV_ERROR_CHECK(err);
                backgroundPadEnd += duration;
            }
            
            foregroundPages.insert(foregroundPages.end(), voicePages.begin(), voicePages.end());
            
            if (endEffect.length())
            {
                foregroundPages.push_back(endEffect);
                
                int64_t duration = 0;
                err = getFileDuration(endEffect, duration);
                AV_ERROR_CHECK(err);
                duration = std::min(duration, MAX_EFFECT_DURATION);
                
                backgroundPadEnd += timeSpan;
                backgroundPadEnd += duration;
            }
            
            // open input file
            AVCombineQueueContext foregroundQueue;
            for (std::string file : foregroundPages)
            {
                int64_t duration = 0;
                err = getFileDuration(file, duration);
                AV_ERROR_CHECK(err);
                
                wholeDuration += duration;
                wholeDuration += timeSpan;
                
                AVFormatContext* format = NULL;
                AVCodecContext* codec = NULL;
                int streamIndex = 0;
                err = openInputFile(file, format, codec, streamIndex);
                if (format)
                {
                    pool.autoRelease([=] {
                        AVFormatContext* f = format;
                        avformat_close_input(&f);
                    });
                }
                if (codec)
                {
                    pool.autoRelease([=]{
                        avcodec_close(codec);
                    });
                }
                AV_ERROR_CHECK(err);
                
                foregroundQueue.inputQueue.push_back(std::make_shared<AVProcessContext>(format, codec, (AVFilterContext*)NULL, streamIndex));
            }
            wholeDuration -= timeSpan;
            
            AVCombineQueueContext backgroundQueue;
            int64_t backgroundDuration = 0;
            int64_t backgroundTrimEnd = 0;
            if (bkgMusicFile.length())
            {
                err = getFileDuration(bkgMusicFile, backgroundDuration);
                AV_ERROR_CHECK(err);
                
                int64_t backgroundWholeDuration = wholeDuration - backgroundDelayStart - backgroundPadEnd;
                backgroundTrimEnd = backgroundDuration - backgroundWholeDuration % backgroundDuration;
                
                int backgroundSegments = ceil((double)backgroundWholeDuration / backgroundDuration);
                for (int i = 0; i < backgroundSegments; ++i)
                {
                    AVFormatContext* backgroundFormat = NULL;
                    AVCodecContext* backgroundCodec = NULL;
                    int backgroundStreamIndex = 0;
                    err = openInputFile(bkgMusicFile, backgroundFormat, backgroundCodec, backgroundStreamIndex);
                    AV_ERROR_CHECK(err);
                    
                    backgroundQueue.inputQueue.push_back(std::make_shared<AVProcessContext>(backgroundFormat, backgroundCodec, (AVFilterContext*)NULL, backgroundStreamIndex));
                }
            }
            
            // open output file
            AVFormatContext* outputFormat = NULL;
            AVCodecContext* outputCodec = NULL;
            err = openOutputFile(outputFile, outputFormat, outputCodec, _outputFileType, _outputBitRate);
            if (outputFormat)
            {
                pool.autoRelease([=] {
                    if (outputFormat->pb)
                        avio_closep(&outputFormat->pb);
                    avformat_free_context(outputFormat);
                });
            }
            if (outputCodec)
            {
                pool.autoRelease([=] {
                    avcodec_close(outputCodec);
                });
            }
            AV_ERROR_CHECK(err);
            AVProcessContext outputContext(outputFormat, outputCodec, NULL, 0);
            
            // init filter
            err = _configFilterGraphForCombineQueues(pool, foregroundQueue, beginEffect.length(), endEffect.length(), timeSpan,
                                                    backgroundQueue, backgroundDuration, backgroundDelayStart, backgroundTrimEnd, backgroundPadEnd, bkgVolume,
                                                    outputContext);
            AV_ERROR_CHECK(err);
            
            // write output file header
            err = avformat_write_header(outputFormat, NULL);
            AV_ERROR_CHECK(err);
            
            // process all data
            std::vector<AVCombineQueueContext> inputQueues;
            inputQueues.push_back(foregroundQueue);
            if (backgroundQueue.inputQueue.size())
                inputQueues.push_back(backgroundQueue);
            
            err = _processAllCombineQueues(inputQueues, outputContext);
            AV_ERROR_CHECK(err);
            
            // write trailer
            err = av_write_trailer(outputFormat);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    virtual int concatAudios(const std::vector<std::string>& audios, double timeSpanSec, const std::string& outputFile)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            int64_t timeSpan = timeSpanSec * outputSampleRate;
            
            // open input file
            std::vector<AVProcessContext> inputContexts;
            for (std::string file : audios)
            {
                AVFormatContext* format = NULL;
                AVCodecContext* codec = NULL;
                int streamIndex = 0;
                err = openInputFile(file, format, codec, streamIndex);
                if (format)
                {
                    pool.autoRelease([=] {
                        AVFormatContext* f = format;
                        avformat_close_input(&f);
                    });
                }
                if (codec)
                {
                    pool.autoRelease([=]{
                        avcodec_close(codec);
                    });
                }
                AV_ERROR_CHECK(err);
                
                inputContexts.push_back(AVProcessContext(format, codec, NULL, streamIndex));
            }
            
            // open output file
            AVFormatContext* outputFormat = NULL;
            AVCodecContext* outputCodec = NULL;
            err = openOutputFile(outputFile, outputFormat, outputCodec, _outputFileType, _outputBitRate);
            if (outputFormat)
            {
                pool.autoRelease([=] {
                    if (outputFormat->pb)
                        avio_closep(&outputFormat->pb);
                    avformat_free_context(outputFormat);
                });
            }
            if (outputCodec)
            {
                pool.autoRelease([=] {
                    avcodec_close(outputCodec);
                });
            }
            AV_ERROR_CHECK(err);
            AVProcessContext outputContext(outputFormat, outputCodec, NULL, 0);
            
            // init filter
            AVFilterGraph* graph = avfilter_graph_alloc();
            ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
            pool.autoRelease([=]
                             {
                                 AVFilterGraph* g = graph;
                                 avfilter_graph_free(&g);
                             });
            
            std::vector<AVFilterContext*> filtersForConcat;
            for (auto it = inputContexts.begin(); it != inputContexts.end(); ++it)
            {
                AVProcessContext& context = *it;
                err = makeInput(graph, context.codec, context.filter);
                AV_ERROR_CHECK(err);
                
                err = makeFormatForOutput(graph, outputContext.codec, context.filter, context.lastFilter);
                AV_ERROR_CHECK(err);
                
                // pad blank span
                if (it != inputContexts.end())
                {
                    err = makePad(graph, context.lastFilter, timeSpan, context.lastFilter);
                    AV_ERROR_CHECK(err);
                }
                
                filtersForConcat.push_back(context.lastFilter);
            }
            
            err = makeConcat(graph, filtersForConcat, outputContext.filter);
            AV_ERROR_CHECK(err);
            err = makeFormatForOutput(graph, outputContext.codec, outputContext.filter, outputContext.filter);
            AV_ERROR_CHECK(err);
            err = makeOutput(graph, outputContext.codec, outputContext.filter, outputContext.filter);
            AV_ERROR_CHECK(err);
            
            err = avfilter_graph_config(graph, NULL);
            AV_ERROR_CHECK(err);
            
            // write output file header
            err = avformat_write_header(outputFormat, NULL);
            AV_ERROR_CHECK(err);
            
            // process all data
            err = processAll(inputContexts, outputContext);
            AV_ERROR_CHECK(err);
            
            // write trailer
            err = av_write_trailer(outputFormat);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    virtual int loudnormAudio(const std::string& inputFile, const std::string& outputFile)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            // open input file
            AVFormatContext* format = NULL;
            AVCodecContext* codec   = NULL;
            int streamIndex         = 0;
            err = openInputFile(inputFile, format, codec, streamIndex);
            if (format)
            {
                pool.autoRelease([=] {
                    AVFormatContext* f = format;
                    avformat_close_input(&f);
                });
            }
            if (codec)
            {
                pool.autoRelease([=]{
                    avcodec_close(codec);
                });
            }
            AV_ERROR_CHECK(err);
            AVProcessContext inputContext(format, codec, NULL, streamIndex);
            
            // open output file
            AVFormatContext* outputFormat = NULL;
            AVCodecContext* outputCodec = NULL;
            err = openOutputFile(outputFile, outputFormat, outputCodec, _outputFileType, _outputBitRate);
            if (outputFormat)
            {
                pool.autoRelease([=] {
                    if (outputFormat->pb)
                        avio_closep(&outputFormat->pb);
                    avformat_free_context(outputFormat);
                });
            }
            if (outputCodec)
            {
                pool.autoRelease([=] {
                    avcodec_close(outputCodec);
                });
            }
            AV_ERROR_CHECK(err);
            AVProcessContext outputContext(outputFormat, outputCodec, NULL, 0);
            
            // init filter
            AVFilterGraph* graph = avfilter_graph_alloc();
            ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
            pool.autoRelease([=]
                             {
                                 AVFilterGraph* g = graph;
                                 avfilter_graph_free(&g);
                             });
            
            err = makeInput(graph, inputContext.codec, inputContext.filter);
            AV_ERROR_CHECK(err);
            
            err = makeLoudNorm(graph, inputContext.filter, inputContext.lastFilter);
            AV_ERROR_CHECK(err);
            
            err = makeFormatForOutput(graph, outputContext.codec, inputContext.lastFilter, inputContext.lastFilter);
            AV_ERROR_CHECK(err);
            
            err = makeOutput(graph, outputContext.codec, inputContext.lastFilter, outputContext.filter);
            AV_ERROR_CHECK(err);
            
            err = avfilter_graph_config(graph, NULL);
            AV_ERROR_CHECK(err);
                        
            // write output file header
            err = avformat_write_header(outputFormat, NULL);
            AV_ERROR_CHECK(err);
            
            // process all data
            std::vector<AVProcessContext> inputs;
            inputs.push_back(inputContext);
            err = processAll(inputs, outputContext);
            AV_ERROR_CHECK(err);
            
            // write trailer
            err = av_write_trailer(outputFormat);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    virtual int convertAudioFile(const std::string& inputFile, const std::string& outputFile)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            // open input file
            AVFormatContext* format = NULL;
            AVCodecContext* codec   = NULL;
            int streamIndex         = 0;
            err = openInputFile(inputFile, format, codec, streamIndex);
            if (format)
            {
                pool.autoRelease([=] {
                    AVFormatContext* f = format;
                    avformat_close_input(&f);
                });
            }
            if (codec)
            {
                pool.autoRelease([=]{
                    avcodec_close(codec);
                });
            }
            AV_ERROR_CHECK(err);
            AVProcessContext inputContext(format, codec, NULL, streamIndex);
            
            // open output file
            AVFormatContext* outputFormat = NULL;
            AVCodecContext* outputCodec = NULL;
            err = openOutputFile(outputFile, outputFormat, outputCodec, _outputFileType, _outputBitRate);
            if (outputFormat)
            {
                pool.autoRelease([=] {
                    if (outputFormat->pb)
                        avio_closep(&outputFormat->pb);
                    avformat_free_context(outputFormat);
                });
            }
            if (outputCodec)
            {
                pool.autoRelease([=] {
                    avcodec_close(outputCodec);
                });
            }
            AV_ERROR_CHECK(err);
            AVProcessContext outputContext(outputFormat, outputCodec, NULL, 0);
            
            // init filter
            AVFilterGraph* graph = avfilter_graph_alloc();
            ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
            pool.autoRelease([=]
                             {
                                 AVFilterGraph* g = graph;
                                 avfilter_graph_free(&g);
                             });
            
            err = makeInput(graph, inputContext.codec, inputContext.filter);
            AV_ERROR_CHECK(err);
            
            err = makeFormatForOutput(graph, outputContext.codec, inputContext.filter, inputContext.lastFilter);
            AV_ERROR_CHECK(err);
            
            err = makeOutput(graph, outputContext.codec, inputContext.lastFilter, outputContext.filter);
            AV_ERROR_CHECK(err);
            
            err = avfilter_graph_config(graph, NULL);
            AV_ERROR_CHECK(err);
            
            // write output file header
            err = avformat_write_header(outputFormat, NULL);
            AV_ERROR_CHECK(err);
            
            // process all data
            std::vector<AVProcessContext> inputs;
            inputs.push_back(inputContext);
            err = processAll(inputs, outputContext);
            AV_ERROR_CHECK(err);
            
            // write trailer
            err = av_write_trailer(outputFormat);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
private:
    int _configFilterGraphForCombineQueues(FFAutoReleasePool& pool,
                                           AVCombineQueueContext& forcegroundQueue,
                                           bool haveBeginEffect,
                                           bool haveEndEffect,
                                           int64_t timeSpan,
                                           AVCombineQueueContext& backgroundQueue,
                                           int64_t backgroundDuration,
                                           int64_t backgroundDelayStart,
                                           int64_t backgroundTrimEnd,
                                           int64_t backgroundPadEnd,
                                           double backgroundvolume,
                                           AVProcessContext& outputContext)
    {
        int err = 0;
        {
            // config forceground input queue
            for (auto it = forcegroundQueue.inputQueue.begin(); it != forcegroundQueue.inputQueue.end(); ++it)
            {
                std::shared_ptr<AVProcessContext> context = *it;
                
                AVFilterGraph* graph = avfilter_graph_alloc();
                ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
                context->pool.autoRelease([=]
                                          {
                                              AVFilterGraph* g = graph;
                                              avfilter_graph_free(&g);
                                          });
                
                err = makeInput(graph, context->codec, context->filter);
                AV_ERROR_CHECK(err);
                
                err = makeFormatForAMIX(graph, context->filter, context->lastFilter);
                AV_ERROR_CHECK(err);
                
                // trim begin effect
                if (haveBeginEffect && (it == forcegroundQueue.inputQueue.begin()))
                {
                    err = makeTrim(graph, context->lastFilter, MAX_EFFECT_DURATION, context->lastFilter);
                    AV_ERROR_CHECK(err);
                    
                    uint64_t fadeDuration = 5 * outputSampleRate;
                    err = makeFade(graph, context->lastFilter, true, MAX_EFFECT_DURATION - fadeDuration, fadeDuration, context->lastFilter);
                    AV_ERROR_CHECK(err);
                }
                
                // pad blank span
                if (it != forcegroundQueue.inputQueue.end())
                {
                    err = makePad(graph, context->lastFilter, timeSpan, context->lastFilter);
                    AV_ERROR_CHECK(err);
                }
                
                err = makeOutput(graph, NULL, context->lastFilter, context->lastFilter);
                AV_ERROR_CHECK(err);
                
                err = avfilter_graph_config(graph, NULL);
                AV_ERROR_CHECK(err);
            }
            
            // config background input queue
            for (auto it = backgroundQueue.inputQueue.begin(); it != backgroundQueue.inputQueue.end(); ++it)
            {
                std::shared_ptr<AVProcessContext> context = *it;
                AVFilterGraph* graph = avfilter_graph_alloc();
                ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
                context->pool.autoRelease([=]
                                          {
                                              AVFilterGraph* g = graph;
                                              avfilter_graph_free(&g);
                                          });
                
                err = makeInput(graph, context->codec, context->filter);
                AV_ERROR_CHECK(err);
                
                err = makeFormatForAMIX(graph, context->filter, context->lastFilter);
                AV_ERROR_CHECK(err);
                
                if (it == backgroundQueue.inputQueue.begin() && backgroundDelayStart)
                {
                    err = makeDelay(graph, context->lastFilter, backgroundDelayStart, context->lastFilter);
                    AV_ERROR_CHECK(err);
                }
                
                if (it == --backgroundQueue.inputQueue.end())
                {
                    int64_t lastDuration = backgroundDuration;
                    if (backgroundTrimEnd)
                    {
                        lastDuration = backgroundDuration - backgroundTrimEnd;
                        if (it == backgroundQueue.inputQueue.begin())
                            lastDuration += backgroundDelayStart;
                        
                        err = makeTrim(graph, context->lastFilter, lastDuration, context->lastFilter);
                        AV_ERROR_CHECK(err);
                    }
                    
                    int64_t fadeDuration = 5 * outputSampleRate;
                    err = makeFade(graph, context->lastFilter, true, std::max<int64_t>(lastDuration - fadeDuration, 0), fadeDuration, context->lastFilter);
                    AV_ERROR_CHECK(err);
                    
                    if (backgroundPadEnd)
                    {
                        err = makePad(graph, context->lastFilter, backgroundPadEnd, context->lastFilter);
                        AV_ERROR_CHECK(err);
                    }
                }
                
                err = makeOutput(graph, NULL, context->lastFilter, context->lastFilter);
                AV_ERROR_CHECK(err);
                
                err = avfilter_graph_config(graph, NULL);
                AV_ERROR_CHECK(err);
            }
            
            // config mix filter graph
            AVFilterGraph* graph = avfilter_graph_alloc();
            ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
            pool.autoRelease([=]
                             {
                                 AVFilterGraph* g = graph;
                                 avfilter_graph_free(&g);
                             });
            
            AVFilterContext* forcegroundInputFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffer"), NULL);
            ERROR_CHECKEX(forcegroundInputFilter, err = AVERROR(ENOMEM));
            err = configInputFilterForAmix(forcegroundInputFilter);
            AV_ERROR_CHECK(err);
            forcegroundQueue.destFilter = forcegroundInputFilter;
            
            AVFilterContext* outputFilter = NULL;
            if (!backgroundQueue.inputQueue.size())
            {
                outputFilter = forcegroundInputFilter;
            }
            else
            {
                AVFilterContext* backgroundInputFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffer"), NULL);
                ERROR_CHECKEX(backgroundInputFilter, err = AVERROR(ENOMEM));
                err = configInputFilterForAmix(backgroundInputFilter);
                AV_ERROR_CHECK(err);
                
                AVFilterContext* backgroundOuputFilter = NULL;
                err = makeVolume(graph, backgroundInputFilter, backgroundvolume, backgroundOuputFilter);
                backgroundQueue.destFilter = backgroundInputFilter;
                
                std::vector<AVFilterContext*> inputFilters;
                inputFilters.push_back(forcegroundInputFilter);
                inputFilters.push_back(backgroundOuputFilter);
                
                
                err = makeMix(graph, inputFilters, outputFilter);
                AV_ERROR_CHECK(err);
            }
            
            // adjust global volume, if multi tracks mix together, the output volume will reduced to prevent overflow.
            if (backgroundQueue.inputQueue.size())
            {
                err = makeVolume(graph, outputFilter, 2., outputFilter);
                AV_ERROR_CHECK(err);
            }
            
            // output format
            err = makeFormatForOutput(graph, outputContext.codec, outputFilter, outputFilter);
            AV_ERROR_CHECK(err);
            
            // output sink
            err = makeOutput(graph, outputContext.codec, outputFilter, outputFilter);
            AV_ERROR_CHECK(err);
            
            err = avfilter_graph_config(graph, NULL);
            AV_ERROR_CHECK(err);
            
            outputContext.filter = outputFilter;
        }
        
    Exit0:
        return err;
    }
    
    int _processAllCombineQueues(std::vector<AVCombineQueueContext>& inputQueues, const AVProcessContext& outputContext)
    {
        int err = 0;
        {
            int64_t framePts = 0;
            int64_t packetPts = 0;
            
            while (true)
            {
                // encode filterd frames
                do
                {
                    AVFrame* filteredFrame = av_frame_alloc();
                    
                    FFAutoReleasePool pool;
                    pool.autoRelease([&filteredFrame]
                                     {
                                         av_frame_free(&filteredFrame);
                                     });
                    
                    err = av_buffersink_get_frame(outputContext.filter, filteredFrame);
                    if (err >= 0)
                    {
                        filteredFrame->pts = framePts;
                        framePts += filteredFrame->nb_samples;
                        
                        err = encodeOneFrame(outputContext.format, outputContext.codec, filteredFrame, packetPts);
                        AV_ERROR_CHECK(err);
                    }
                }
                while (err >= 0);
                
                // need more input
                if (AVERROR(EAGAIN) == err)
                {
                    // find lack source inputs
                    std::vector<int> lackSourceInputs;
                    for (int i = 0; i < inputQueues.size(); ++i)
                    {
                        if (av_buffersrc_get_nb_failed_requests(inputQueues[i].destFilter) > 0)
                            lackSourceInputs.push_back(i);
                    }
                    
                    // decode frames and fill in to input filter
                    for (int i : lackSourceInputs)
                    {
                        AVCombineQueueContext& queue = inputQueues[i];
                        assert(queue.inputQueue.size());
                        std::shared_ptr<AVProcessContext> inputContext = queue.inputQueue.front();
                        
                        bool finished = false;
                        for (int j = 0; j < 128 && !finished; ++j)
                        {
                            AVFrame* inputFrame = av_frame_alloc();
                            ERROR_CHECKEX(inputFrame, err = AVERROR(ENOMEM));
                            
                            FFAutoReleasePool pool;
                            pool.autoRelease([&inputFrame]
                                             {
                                                 av_frame_free(&inputFrame);
                                             });
                            
                            err = decodeOneFrame(inputContext->format,
                                                 inputContext->codec,
                                                 inputContext->streamIndex,
                                                 inputFrame,
                                                 inputContext->currentPTS,
                                                 finished);
                            AV_ERROR_CHECK(err);
                            
                            if (!finished)
                            {
                                err = av_buffersrc_add_frame(inputContext->filter, inputFrame);
                                AV_ERROR_CHECK(err);
                            }
                        }
                        
                        // end input
                        if (finished)
                        {
                            err = av_buffersrc_add_frame(inputContext->filter, NULL);
                            AV_ERROR_CHECK(err);
                        }
                        
                        {
                            do
                            {
                                FFAutoReleasePool pool;
                                AVFrame* queueFrame = av_frame_alloc();
                                pool.autoRelease([&queueFrame]
                                                 {
                                                     av_frame_free(&queueFrame);
                                                 });
                                
                                err = av_buffersink_get_frame(inputContext->lastFilter, queueFrame);
                                if (err >= 0)
                                {
                                    err = av_buffersrc_add_frame(queue.destFilter, queueFrame);
                                    AV_ERROR_CHECK(err);
                                }
                                
                            } while (err >= 0);
                            err = 0;
                        }
                        
                        
                        if (finished)
                        {
                            queue.inputQueue.erase(queue.inputQueue.begin());
                            
                            if (!queue.inputQueue.size())
                            {
                                err = av_buffersrc_add_frame(queue.destFilter, NULL);
                                AV_ERROR_CHECK(err);
                            }
                        }
                    }
                }
                // flush encoder
                else if ((AVERROR(ENOMEM) == err) || AVERROR_EOF == err)
                {
                    err = encodeFlush(outputContext.format, outputContext.codec, packetPts);
                    AV_ERROR_CHECK(err);
                    break;
                }
                
                // other errors
                AV_ERROR_CHECK(err);
            }
        }
        
    Exit0:
        return err;
    }
};

IFFAudioMixing* FFAudioMixingFactory::createInstance()
{
    return new FFAudioMixing();
}
