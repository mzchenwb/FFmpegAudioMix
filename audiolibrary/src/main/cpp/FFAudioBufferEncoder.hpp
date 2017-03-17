//
//  FFAudioBufferEncoder.hpp
//  FFAudioMixing
//
//  Created by fanzhang on 16/9/18.
//  Copyright © 2016年 bbo. All rights reserved.
//

#ifndef FFAudioBufferEncoder_hpp
#define FFAudioBufferEncoder_hpp

#include <string>
#include "FFAudioHelper.hpp"
typedef std::vector<uint8_t> XBuffer;
typedef std::deque<std::shared_ptr<XBuffer>> XBufferQueue;

class FFAudioBufferEncoder
{
private:
    std::string _outputFile;
    std::string _outputFileType;
    int _outputBitrate;
    FFAudioHelper::AVProcessContext _outputContext;
    FFAudioHelper::AVProcessContext _inputContext;
    int64_t _framePts;
    int64_t _packetPts;
    FFAutoReleasePool _pool;
    XBufferQueue queue;
public:
    FFAudioBufferEncoder(const char* outputFile, const char* outputFileType, const int outputBitRate);
    
    int beginInput();
    int appendData(const uint8_t* data, int size);
    int endInput();

    void write_queue(const uint8_t *data, int len);

    XBuffer *read_queue();
};

#endif /* FFAudioBufferEncoder_hpp */
