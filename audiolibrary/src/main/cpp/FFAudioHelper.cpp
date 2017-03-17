//
//  FFAudioHelper.cpp
//  playbook
//
//  Created by fanzhang on 16/8/30.
//  Copyright © 2016年 baobeiyixiaoshi. All rights reserved.
//

#include "FFAudioHelper.hpp"
#include <cassert>

namespace
{
    const char* ID3Magic = "ID3";
}

namespace FFAudioHelper
{
    AVProcessContext::AVProcessContext()
    : format(NULL)
    , codec(NULL)
    , filter(NULL)
    , streamIndex(0)
    , lastFilter(0)
    , currentPTS(0)
    {
        
    }
    
    AVProcessContext::AVProcessContext(AVFormatContext* format_, AVCodecContext* codec_, AVFilterContext* filter_, int streamIndex_)
    : format(format_)
    , codec(codec_)
    , filter(filter_)
    , streamIndex(streamIndex_)
    , lastFilter(NULL)
    , currentPTS(0)
    {
        
    }
    
    int getFileDuration(const std::string& file, int64_t& duration)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            // open file
            AVFormatContext* format = NULL;
            pool.autoRelease([&format]
                             {
                                 if (format)
                                     avformat_close_input(&format);
                             });
            
            AVCodecContext* codec = NULL;
            pool.autoRelease([&codec]
                             {
                                 if (codec)
                                 {
                                     avcodec_close(codec);
                                     codec = NULL;
                                 }
                             });
            
            int streamIndex = 0;
            err = openInputFile(file, format, codec, streamIndex);
            AV_ERROR_CHECK(err);
            
            // quick method
            AVStream* stream = format->streams[streamIndex];
            if ((codec->sample_rate == outputSampleRate) && codec->frame_size && stream->nb_frames)
            {
                duration = stream->duration;
                QUIT();
            }
            
            // no accurate duration info from meta data, have to decode the whole file
            // filter graph
            AVFilterGraph* graph = NULL;
            pool.autoRelease([&graph]
                             {
                                 if (graph)
                                     avfilter_graph_free(&graph);
                             });
            
            graph = avfilter_graph_alloc();
            ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
            
            AVFilterContext* inputFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffer"), "input");
            ERROR_CHECKEX(inputFilter, err = AVERROR(ENOMEM));
            err = configInputFilter(inputFilter, codec);
            AV_ERROR_CHECK(err);
            
            // input format
            AVFilterContext* aFormat = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("aformat"), "format1");
            ERROR_CHECKEX(aFormat, err = AVERROR(ENOMEM));
            err = configFormatFilterForAmix(aFormat);
            AV_ERROR_CHECK(err);
            
            // output sink
            AVFilterContext* outputFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffersink"), "output");
            ERROR_CHECKEX(outputFilter, err = AVERROR(ENOMEM));
            err = avfilter_init_str(outputFilter, NULL);
            AV_ERROR_CHECK(err);
            
            //
            err = avfilter_link(inputFilter, 0, aFormat, 0);
            AV_ERROR_CHECK(err);
            err = avfilter_link(aFormat, 0, outputFilter, 0);
            AV_ERROR_CHECK(err);
            
            err = avfilter_graph_config(graph, NULL);
            AV_ERROR_CHECK(err);
            
            
            // decode frames to get real duration
            int64_t framePts = 0;
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
                    
                    err = av_buffersink_get_frame(outputFilter, filteredFrame);
                    if (err >= 0)
                    {
                        framePts += filteredFrame->nb_samples;
                        AV_ERROR_CHECK(err);
                    }
                }
                while (err >= 0);
                
                // need more input
                if (AVERROR(EAGAIN) == err)
                {
                    // decode frames and fill in to input filter
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
                        
                        int64_t pts = 0;
                        err = decodeOneFrame(format, codec, streamIndex, inputFrame, pts, finished);
                        AV_ERROR_CHECK(err);
                        
                        if (!finished)
                        {
                            err = av_buffersrc_add_frame(inputFilter, inputFrame);
                            AV_ERROR_CHECK(err);
                        }
                    }
                    
                    // end input
                    if (finished)
                    {
                        err = av_buffersrc_add_frame(inputFilter, NULL);
                        AV_ERROR_CHECK(err);
                    }
                }
                // flush encoder
                else if (AVERROR_EOF == err)
                {
                    err = 0;
                    duration = framePts;
                    break;
                }
                // other errors
                AV_ERROR_CHECK(err);
            }
        }
        
    Exit0:
        return err;
    }
    
    std::string getErrorText(int err)
    {
        std::vector<char> buffer;
        buffer.resize(256);
        av_strerror(err, &buffer.front(), buffer.size());
        std::string msg = &buffer.front();
        return msg;
    }
    
    int openInputFile(const std::string& inputFile, AVFormatContext*& formatContext, AVCodecContext*& codecContext, int& streamIndex)
    {
        int err = 0;
        {
            err = avformat_open_input(&formatContext, inputFile.c_str(), NULL, NULL);
            AV_ERROR_CHECK(err);
            
            err = avformat_find_stream_info(formatContext, NULL);
            AV_ERROR_CHECK(err);
            
            AVCodec* codec = NULL;
            streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
            ERROR_CHECK(streamIndex >= 0);
            
            err = avcodec_open2(formatContext->streams[streamIndex]->codec, codec, NULL);
            AV_ERROR_CHECK(err);
            
            codecContext = formatContext->streams[streamIndex]->codec;
            if (!codecContext->channel_layout)
                codecContext->channel_layout = av_get_default_channel_layout(codecContext->channels);
        }
        
    Exit0:
        return err;
    }
    
    int openOutputFile(const std::string& outputFile,
                              AVFormatContext*& formatContext,
                              AVCodecContext*& codecContext,
                              const std::string& fileType,
                              const int bitrate)
    {
        int err = 0;
        {
            AVIOContext* ioContext = NULL;
            err = avio_open(&ioContext, outputFile.c_str(), AVIO_FLAG_WRITE);
            AV_ERROR_CHECK(err);
            
            formatContext = avformat_alloc_context();
            ERROR_CHECKEX(formatContext, err = AVERROR(ENOMEM));
            
            formatContext->pb = ioContext;
            formatContext->oformat = av_guess_format(NULL, fileType.c_str(), NULL);
            ERROR_CHECKEX(formatContext->oformat, err = AVERROR_MUXER_NOT_FOUND);
            
            av_strlcpy(formatContext->filename, outputFile.c_str(), sizeof((formatContext)->filename));
            
            AVCodec* codec = avcodec_find_encoder(formatContext->oformat->audio_codec);
            ERROR_CHECKEX(codec, err = AVERROR_ENCODER_NOT_FOUND);
            
            AVStream *stream = avformat_new_stream(formatContext, codec);
            ERROR_CHECKEX(stream, err = AVERROR_UNKNOWN);
            
            codecContext = stream->codec;
            codecContext->channels       = outputAudioChannelNum;
            codecContext->channel_layout = av_get_default_channel_layout(outputAudioChannelNum);
            codecContext->sample_rate    = outputSampleRate;
            codecContext->sample_fmt     = codec->sample_fmts[0];
            codecContext->bit_rate       = bitrate;
            
            /** Allow the use of the experimental AAC encoder */
            codecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
            
            /** Set the sample rate for the container. */
            stream->time_base.den = codecContext->sample_rate;
            stream->time_base.num = 1;
            
            /**
             * Some container formats (like MP4) require global headers to be present
             * Mark the encoder so that it behaves accordingly.
             */
            if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
                codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            
            /** Open the encoder for the audio stream to use it later. */
            err = avcodec_open2(codecContext, codec, NULL);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int configFilterGraphForMixing(const int64_t wholeDuration,
                                                   const AVCodecContext* inputCodec1, AVFilterContext*& inputFilter1,
                                                   const AVCodecContext* inputCodec2, AVFilterContext*& inputFilter2,
                                                   const AVCodecContext* outputCodec, AVFilterContext*& outputFilter,
                                                   AVFilterGraph*& graph)
    {
        int err = 0;
        {
            graph = avfilter_graph_alloc();
            ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
            
            // input 1
            err = makeInput(graph, inputCodec1, inputFilter1);
            AV_ERROR_CHECK(err);
            
            AVFilterContext* aFormat1 = NULL;
            err = makeFormatForAMIX(graph, inputFilter1, aFormat1);
            AV_ERROR_CHECK(err);
            
            AVFilterContext* aPad1 = NULL;
            err = makePadWhole(graph, aFormat1, wholeDuration, aPad1);
            AV_ERROR_CHECK(err);
            
            // input2
            err = makeInput(graph, inputCodec2, inputFilter2);
            AV_ERROR_CHECK(err);
            
            AVFilterContext* aFormat2 = NULL;
            err = makeFormatForAMIX(graph, inputFilter2, aFormat2);
            AV_ERROR_CHECK(err);
            
            AVFilterContext* aPad2 = NULL;
            err = makePadWhole(graph, aFormat2, wholeDuration, aPad2);
            AV_ERROR_CHECK(err);
            
            // amix filter
            AVFilterContext* mixFilter = NULL;
            std::vector<AVFilterContext*> inputs;
            inputs.push_back(aPad1);
            inputs.push_back(aPad2);
            err = makeMix(graph, inputs, mixFilter);
            AV_ERROR_CHECK(err);
            
            // output format
            AVFilterContext* aFormatOut = NULL;
            err = makeFormatForOutput(graph, outputCodec, mixFilter, aFormatOut);
            AV_ERROR_CHECK(err);
            
            // output sink
            err = makeOutput(graph, outputCodec, aFormatOut, outputFilter);
            AV_ERROR_CHECK(err);
            
            err = avfilter_graph_config(graph, NULL);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int decodeOneFrame(AVFormatContext* inputFormat, AVCodecContext* inputCodec, int inputStream, AVFrame* frame, int64_t& globalPTS, bool& finished)
    {
        int err = 0;
        {
            bool dataPresent = false;
            while (!dataPresent && !finished)
            {
                err = tryDecodeOneFrame(inputFormat, inputCodec, inputStream, frame, dataPresent, finished);
                AV_ERROR_CHECK(err);
            }
            
            // must config correct PTS value for input frames, otherwise some filters like afade will not working
            if (!finished)
            {
                frame->pts = globalPTS;
                globalPTS += frame->nb_samples;
            }
        }
        
    Exit0:
        return err;
    }
    
    int tryDecodeOneFrame(AVFormatContext* inputFormat, AVCodecContext* inputCodec, int inputStream, AVFrame* frame, bool& dataPresent, bool& finished)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            dataPresent = false;
            finished = false;
            
            AVPacket packet = {0};
            av_init_packet(&packet);
            pool.autoRelease([&packet]
                             {
                                 av_packet_unref(&packet);
                             });
            
            err = av_read_frame(inputFormat, &packet);
            if (AVERROR_EOF == err)
            {
                finished = true;
                err = 0;
            }
            AV_ERROR_CHECK(err);
            
            if (((packet.stream_index == inputStream) && // skip other streams like artwork picture
                 ((packet.size < strlen(ID3Magic)) || !packetIsID3(&packet))) // skip ID3 data
                || finished) // flush decoder
            {
                int gotFrame = 0;
                err = avcodec_decode_audio4(inputCodec, frame, &gotFrame, &packet);
                AV_ERROR_CHECK(err);
                
                assert(err == packet.size);
                dataPresent = gotFrame;
            }
            
            if (finished && dataPresent)
                finished = false;
        }
        
    Exit0:
        return err;
    }
    
    int encodeOneFrame(AVFormatContext* outputFormat, AVCodecContext* outputCodec, AVFrame* frame, int64_t& packetPts)
    {
        int err = 0;
        {
            FFAutoReleasePool pool;
            
            AVPacket packet = {0};
            av_init_packet(&packet);
            pool.autoRelease([&packet]
                             {
                                 av_packet_unref(&packet);
                             });
            
            int gotPacket = 0;
            err = avcodec_encode_audio2(outputCodec, &packet, frame, &gotPacket);
            AV_ERROR_CHECK(err);
            
            if (gotPacket)
            {
                packet.pts = packetPts;
                packet.dts = packetPts;
                packetPts += packet.duration;
                
                err = av_interleaved_write_frame(outputFormat, &packet);
                AV_ERROR_CHECK(err);
            }
        }
        
    Exit0:
        return err;
    }
    
    int encodeFlush(AVFormatContext* outputFormat, AVCodecContext* outputCodec, int64_t& packetPts)
    {
        int err = 0;
        {
            int64_t oldPts = 0;
            do
            {
                oldPts = packetPts;
                err = encodeOneFrame(outputFormat, outputCodec, NULL, packetPts);
                AV_ERROR_CHECK(err);
            }
            while (oldPts < packetPts);
        }
        
    Exit0:
        return err;
    }
    
    int processAll(std::vector<AVProcessContext>& inputContexts, const AVProcessContext& outputContext)
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
                    FFAutoReleasePool pool;
                    
                    AVFrame* filteredFrame = av_frame_alloc();
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
                    for (int i = 0; i < inputContexts.size(); ++i)
                    {
                        if (av_buffersrc_get_nb_failed_requests(inputContexts[i].filter) > 0)
                            lackSourceInputs.push_back(i);
                    }
                    
                    // decode frames and fill in to input filter
                    for (int i : lackSourceInputs)
                    {
                        AVProcessContext& inputContext = inputContexts[i];
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
                            
                            err = decodeOneFrame(inputContext.format, inputContext.codec, inputContext.streamIndex, inputFrame, inputContext.currentPTS, finished);
                            AV_ERROR_CHECK(err);
                            
                            if (!finished)
                            {
                                err = av_buffersrc_add_frame(inputContext.filter, inputFrame);
                                AV_ERROR_CHECK(err);
                            }
                        }
                        
                        // end input
                        if (finished)
                        {
                            err = av_buffersrc_add_frame(inputContext.filter, NULL);
                            AV_ERROR_CHECK(err);
                        }
                    }
                }
                // flush encoder
                else if ((AVERROR(ENOMEM) == err) || AVERROR_EOF == err)
                {
                    err = encodeFlush(outputContext.format, outputContext.codec, packetPts);
                    break;
                }
                
                // other errors
                AV_ERROR_CHECK(err);
            }
        }
        
    Exit0:
        return err;
    }
    
    int makeInput(AVFilterGraph* graph, const AVCodecContext* codec, AVFilterContext*& input)
    {
        int err = 0;
        {
            input = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffer"), NULL);
            ERROR_CHECKEX(input, err = AVERROR(ENOMEM));
            err = configInputFilter(input, codec);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int makeOutput(AVFilterGraph* graph, const AVCodecContext* codec, AVFilterContext* input, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* outputFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffersink"), NULL);
            ERROR_CHECKEX(outputFilter, err = AVERROR(ENOMEM));
            err = avfilter_init_str(outputFilter, NULL);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, outputFilter, 0);
            AV_ERROR_CHECK(err);
            
            if (codec && codec->frame_size > 0)
                av_buffersink_set_frame_size(outputFilter, codec->frame_size);
            
            output = outputFilter;
        }
        
    Exit0:
        return err;
    }
    
    int makeFormatForAMIX(AVFilterGraph* graph, AVFilterContext* input, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* format = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("aformat"), NULL);
            ERROR_CHECKEX(format, err = AVERROR(ENOMEM));
            err = configFormatFilterForAmix(format);
            AV_ERROR_CHECK(err);
            
            output = format;
            err = avfilter_link(input, 0, output, 0);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int makeFormatForOutput(AVFilterGraph* graph, const AVCodecContext* codec, AVFilterContext* input, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* format = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("aformat"), NULL);
            ERROR_CHECKEX(format, err = AVERROR(ENOMEM));
            err = configFormatFilter(format, codec);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, format, 0);
            AV_ERROR_CHECK(err);
            
            output = format;
        }
        
    Exit0:
        return err;
    }
    
    int makePad(AVFilterGraph* graph, AVFilterContext* input, int64_t padDuration, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* pad = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("apad"), NULL);
            ERROR_CHECKEX(pad, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "pad_len=%lld", padDuration);
            err = avfilter_init_str(pad, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, pad, 0);
            AV_ERROR_CHECK(err);
            
            output = pad;
        }
        
    Exit0:
        return err;
    }
    
    int makePadWhole(AVFilterGraph* graph, AVFilterContext* input, int64_t wholeDuration, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* pad = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("apad"), NULL);
            ERROR_CHECKEX(pad, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "whole_len=%lld", wholeDuration);
            err = avfilter_init_str(pad, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, pad, 0);
            AV_ERROR_CHECK(err);
            
            output = pad;
        }
        
    Exit0:
        return err;
    }
    
    int makeTrim(AVFilterGraph* graph, AVFilterContext* input, int64_t wholeDuration, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* trim = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("atrim"), NULL);
            ERROR_CHECKEX(trim, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "end_sample=%lld", wholeDuration);
            err = avfilter_init_str(trim, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, trim, 0);
            AV_ERROR_CHECK(err);
            
            output = trim;
        }
        
    Exit0:
        return err;
    }
    
    int makeFade(AVFilterGraph* graph, AVFilterContext* input, bool fadeOut, int64_t start, int64_t nb, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* afade = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("afade"), NULL);
            ERROR_CHECKEX(afade, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "type=%d:start_sample=%lld:nb_samples=%lld", (int)fadeOut, start, nb);
            err = avfilter_init_str(afade, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, afade, 0);
            AV_ERROR_CHECK(err);
            
            output = afade;
        }
        
    Exit0:
        return err;
    }
    
    int makeDelay(AVFilterGraph* graph, AVFilterContext* input, int64_t delayDuration, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* delay = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("adelay"), NULL);
            ERROR_CHECKEX(delay, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "delays=%d", (int)((double)delayDuration / outputSampleRate * 1000.0));
            err = avfilter_init_str(delay, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, delay, 0);
            AV_ERROR_CHECK(err);
            
            output = delay;
        }
        
    Exit0:
        return err;
    }
    
    int makeVolume(AVFilterGraph* graph, AVFilterContext* input, double volume, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* volumeFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("volume"), NULL);
            ERROR_CHECKEX(volumeFilter, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "volume=%f", volume);
            err = avfilter_init_str(volumeFilter, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, volumeFilter, 0);
            AV_ERROR_CHECK(err);
            
            output = volumeFilter;
        }
        
    Exit0:
        return err;
    }
    
    int makeSplit(AVFilterGraph* graph, AVFilterContext* input, int count, std::vector<AVFilterContext*>& outputs)
    {
        int err = 0;
        {
            AVFilterContext* split = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("asplit"), NULL);
            ERROR_CHECKEX(split, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "%d", count);
            err = avfilter_init_str(split, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, split, 0);
            AV_ERROR_CHECK(err);
            
            for (int i = 0; i < count; ++i)
            {
                AVFilterContext* null = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("anull"), NULL);
                ERROR_CHECKEX(null, err = AVERROR(ENOMEM));
                
                err = avfilter_link(split, i, null, 0);
                AV_ERROR_CHECK(err);
                
                outputs.push_back(null);
            }
        }
        
    Exit0:
        return err;
    }
    
    int makeMix(AVFilterGraph* graph, const std::vector<AVFilterContext*>& inputs, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* mixFilter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("amix"), NULL);
            ERROR_CHECKEX(mixFilter, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "inputs=%d", (int)inputs.size());
            err = avfilter_init_str(mixFilter, options);
            AV_ERROR_CHECK(err);
            
            for (int i = 0; i < inputs.size(); ++i)
            {
                err = avfilter_link(inputs[i], 0, mixFilter, i);
                AV_ERROR_CHECK(err);
            }
            
            output = mixFilter;
        }
        
    Exit0:
        return err;
    }
    
    int makeConcat(AVFilterGraph* graph, const std::vector<AVFilterContext*>& inputs, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* filter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("concat"), NULL);
            ERROR_CHECKEX(filter, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "n=%d:v=0:a=1", (int)inputs.size());
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
            
            for (int i = 0; i < inputs.size(); ++i)
            {
                err = avfilter_link(inputs[i], 0, filter, i);
                AV_ERROR_CHECK(err);
            }
            
            output = filter;
        }
        
    Exit0:
        return err;
    }
    
    int makeLoudNorm(AVFilterGraph* graph, AVFilterContext* input, AVFilterContext*& output)
    {
        int err = 0;
        {
            AVFilterContext* filter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("loudnorm"), NULL);
            ERROR_CHECKEX(filter, err = AVERROR(ENOMEM));
            
            char options[128] = {0};
            snprintf(options, sizeof(options), "I=%.1f:TP=%.1f:LRA=%d:dual_mono=false", -16.0, -2.0, 7);
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
            
            err = avfilter_link(input, 0, filter, 0);
            AV_ERROR_CHECK(err);
            
            output = filter;
        }
        
    Exit0:
        return err;
    }
    
    int configInputFilter(AVFilterContext* filter, const AVCodecContext* codec)
    {
        int err = 0;
        {
            char options[128] = {0};
            snprintf(options, sizeof(options),
                     "sample_fmt=%s:sample_rate=%d:channel_layout=0x%x:time_base=1/%d",
                     av_get_sample_fmt_name(codec->sample_fmt),
                     codec->sample_rate,
                     (int)codec->channel_layout,
                     codec->sample_rate);
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int configInputFilter(AVFilterContext* filter, AVSampleFormat sampleFormat, int sampleRate, int  channels)
    {
        int err = 0;
        {
            char options[128] = {0};
            snprintf(options, sizeof(options),
                     "sample_fmt=%s:sample_rate=%d:channel_layout=0x%x:time_base=1/%d",
                     av_get_sample_fmt_name(sampleFormat),
                     sampleRate,
                     (int)av_get_default_channel_layout(channels),
                     sampleRate);
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int configInputFilterForAmix(AVFilterContext* filter)
    {
        int err = 0;
        {
            char options[128] = {0};
            snprintf(options, sizeof(options),
                     "sample_fmt=%s:sample_rate=%d:channel_layout=0x%x:time_base=1/%d",
                     av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP),
                     outputSampleRate,
                     (int)av_get_default_channel_layout(1),
                     outputSampleRate);
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    int configFormatFilter(AVFilterContext* filter, const AVCodecContext* codec)
    {
        int err = 0;
        {
            char options[128] = {0};
            snprintf(options, sizeof(options),
                     "sample_fmts=%s:sample_rates=%d:channel_layouts=0x%x",
                     av_get_sample_fmt_name(codec->sample_fmt),
                     codec->sample_rate,
                     (int)codec->channel_layout);
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    /*
     The amix filter can only accept sample format: [44100Hz/fltp/mono], 
     the frames decoded from diffrent file formats have to resample to this format by aformat filter,
     otherwise the filterd output voice should be wrong !!!
     */
    int configFormatFilterForAmix(AVFilterContext* filter)
    {
        int err = 0;
        {
            char options[128] = {0};
            snprintf(options, sizeof(options),
                     "sample_fmts=%s:sample_rates=%d:channel_layouts=0x%x",
                     av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP),
                     outputSampleRate,
                     (int)av_get_default_channel_layout(1));
            
            err = avfilter_init_str(filter, options);
            AV_ERROR_CHECK(err);
        }
        
    Exit0:
        return err;
    }
    
    bool packetIsID3(const AVPacket* packet)
    {
        return (0 == memcmp(packet->data, ID3Magic, strlen(ID3Magic)));
    }
}
