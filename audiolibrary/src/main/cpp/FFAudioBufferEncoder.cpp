//
//  FFAudioBufferEncoder.cpp
//  FFAudioMixing
//
//  Created by fanzhang on 16/9/18.
//  Copyright © 2016年 bbo. All rights reserved.
//

#include "FFAudioBufferEncoder.hpp"

using namespace FFAudioHelper;

FFAudioBufferEncoder::FFAudioBufferEncoder(const char* outputFile, const char* outputFileType, const int outputBitRate)
: _outputFile(outputFile)
, _outputFileType(outputFileType)
, _outputBitrate(outputBitRate)
, _framePts(0)
, _packetPts(0)
{

}

int FFAudioBufferEncoder::beginInput()
{
    int err = 0;
    {
        // open output file
        err = openOutputFile(_outputFile, _outputContext.format, _outputContext.codec, _outputFileType, _outputBitrate);
        if (_outputContext.format)
        {
            _pool.autoRelease([=] {
                if (_outputContext.format->pb)
                    avio_closep(&_outputContext.format->pb);
                avformat_free_context(_outputContext.format);
            });
        }
        if (_outputContext.codec)
        {
            _pool.autoRelease([=] {
                avcodec_close(_outputContext.codec);
            });
        }
        AV_ERROR_CHECK(err);

        // init filter
        AVFilterGraph* graph = avfilter_graph_alloc();
        ERROR_CHECKEX(graph, err = AVERROR(ENOMEM));
        _pool.autoRelease([=]
                         {
                             AVFilterGraph* g = graph;
                             avfilter_graph_free(&g);
                         });

        _inputContext.filter = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffer"), NULL);
        ERROR_CHECKEX(_inputContext.filter, err = AVERROR(ENOMEM));
        err = configInputFilter(_inputContext.filter, AV_SAMPLE_FMT_S16, outputSampleRate, outputAudioChannelNum);
        AV_ERROR_CHECK(err);

//        err = makeLoudNorm(graph, _inputContext.filter, _inputContext.lastFilter);
//        AV_ERROR_CHECK(err);

        err = makeFormatForOutput(graph, _outputContext.codec, _inputContext.filter, _inputContext.lastFilter);
        AV_ERROR_CHECK(err);
        err = makeOutput(graph, _outputContext.codec, _inputContext.lastFilter, _outputContext.filter);
        AV_ERROR_CHECK(err);

        err = avfilter_graph_config(graph, NULL);
        AV_ERROR_CHECK(err);

        // write output file header
        err = avformat_write_header(_outputContext.format, NULL);
        AV_ERROR_CHECK(err);
    }

Exit0:
    return err;
}

void FFAudioBufferEncoder::write_queue(const uint8_t* data, int len)
{
    XBuffer *buffer = new XBuffer();
    buffer->resize(len);

    uint8_t *head = &buffer->front();
    memcpy(head, data, len);

    queue.push_back(std::shared_ptr<XBuffer>(buffer));
}

XBuffer* FFAudioBufferEncoder::read_queue()
{
    int per_size = 2 * 1024;
    int exist_size = 0;

    std::deque<std::shared_ptr<XBuffer>>::iterator pos;
    for (pos = queue.begin(); pos != queue.end(); pos++)
    {
        exist_size += (*pos)->size();
    }

    if (exist_size < per_size)
        return NULL;

    int multi_size = exist_size - (exist_size % per_size);

    XBuffer *buffer = new XBuffer();
    buffer->resize(multi_size);

    int copy_size = 0;
    while(copy_size < multi_size)
    {
        std::shared_ptr<XBuffer> &q_buffer = queue.front();
        if (q_buffer->size() + copy_size > multi_size) //缓存的大小大于需要的大小
        {
            int need_size = multi_size - copy_size;
            memcpy(&buffer->front() + copy_size, &q_buffer->front(), need_size);

            int left_size = q_buffer->size() - need_size;

            XBuffer *newBuffer = new XBuffer();
            newBuffer->resize(left_size);
            memcpy(&newBuffer->front(), &q_buffer->front() + need_size, newBuffer->size());

            queue.pop_front();
            queue.push_front(std::shared_ptr<XBuffer>(newBuffer));

            copy_size = multi_size;
        }
        else //不足
        {
            memcpy(&buffer->front() + copy_size, &q_buffer->front(), q_buffer->size());
            copy_size += q_buffer->size();

            queue.pop_front();
        }
    }

    return buffer;
}

int FFAudioBufferEncoder::appendData(const uint8_t* data, int len)
{
    write_queue(data, len);

    XBuffer* buffer = read_queue();
    if (!buffer || buffer->size() <= 0)
        return 0;

    int size = buffer->size();

    int err = 0;
    {
        FFAutoReleasePool pool;

        // input frame
        AVFrame* frame = av_frame_alloc();
        pool.autoRelease([&frame]
                         {
                             av_frame_free(&frame);
                         });

        frame->data[0]        = (uint8_t*)av_malloc(size);
        frame->format         = AV_SAMPLE_FMT_S16;
        frame->channels       = outputAudioChannelNum;
        frame->channel_layout = av_get_default_channel_layout(outputAudioChannelNum);
        frame->nb_samples     = size / (16 / 8 * outputAudioChannelNum);
        frame->sample_rate    = outputSampleRate;

        err = avcodec_fill_audio_frame(frame, frame->channels, (enum AVSampleFormat)frame->format, &buffer->front(), size, true);
        AV_ERROR_CHECK(err);

        err = av_buffersrc_add_frame(_inputContext.filter, frame);
        AV_ERROR_CHECK(err);

        // output & encode frame
        do
        {
            FFAutoReleasePool pool;

            AVFrame* filteredFrame = av_frame_alloc();
            pool.autoRelease([&filteredFrame]
                             {
                                 av_frame_free(&filteredFrame);
                             });

            err = av_buffersink_get_frame(_outputContext.filter, filteredFrame);
            if (err >= 0)
            {
                filteredFrame->pts = _framePts;
                _framePts += filteredFrame->nb_samples;

                err = encodeOneFrame(_outputContext.format, _outputContext.codec, filteredFrame, _packetPts);
                AV_ERROR_CHECK(err);
            }
        }
        while (err >= 0);
        err = 0;
    }

Exit0:
    return err;
}

int FFAudioBufferEncoder::endInput()
{
    int err = 0;
    {
        err = av_buffersrc_add_frame(_inputContext.filter, NULL);
        AV_ERROR_CHECK(err);

        // output & encode frame
        do
        {
            FFAutoReleasePool pool;

            AVFrame* filteredFrame = av_frame_alloc();
            pool.autoRelease([&filteredFrame]
                             {
                                 av_frame_free(&filteredFrame);
                             });

            err = av_buffersink_get_frame(_outputContext.filter, filteredFrame);
            if (err >= 0)
            {
                filteredFrame->pts = _framePts;
                _framePts += filteredFrame->nb_samples;

                err = encodeOneFrame(_outputContext.format, _outputContext.codec, filteredFrame, _packetPts);
                AV_ERROR_CHECK(err);
            }
        }
        while (err >= 0);

        err = encodeFlush(_outputContext.format, _outputContext.codec, _packetPts);
        AV_ERROR_CHECK(err);

        // write trailer
        err = av_write_trailer(_outputContext.format);
        AV_ERROR_CHECK(err);

        queue.clear();
    }

Exit0:
    return err;
}
