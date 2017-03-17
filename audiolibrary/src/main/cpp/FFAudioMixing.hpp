//
//  FFAudioMixing.hpp
//  FFAudioMixing
//
//  Created by fanzhang on 16/3/27.
//  Copyright © 2016年 bbo. All rights reserved.
//

#ifndef FFAudioMixing_hpp
#define FFAudioMixing_hpp

#include <string>
#include <vector>

namespace
{
    const char* OUTPUT_FILE_TYPE = ".mp3";
    const int OUTPUT_BIT_RATE    = 160000;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

struct IFFAudioMixing
{
    virtual void init(const char* outputFileType = OUTPUT_FILE_TYPE, const int outputBitRate = OUTPUT_BIT_RATE) = 0;
    virtual void destroy() = 0;
    
    virtual int mixAudio(const std::string& inputFile1, const std::string inputFile2, const std::string& outputFile) = 0;
    virtual int combineAudios(const std::string&                beginEffect,
                              const std::string&                endEffect,
                              bool                              haveIntroPage,
                              bool                              haveEndingPage,
                              const std::vector<std::string>&   voicePages,
                              double                            timeSpanSec,
                              const std::string&                bkgMusicFile,
                              double                            bkgVolume,
                              const std::string&                outputFile) = 0;
    virtual int concatAudios(const std::vector<std::string>& audios, double timeSpanSec, const std::string& outputFile) = 0;
    virtual int loudnormAudio(const std::string& inputFile, const std::string& outputFile) = 0;
    virtual int convertAudioFile(const std::string& inputFile, const std::string& outputFile) = 0;
};

struct FFAudioMixingFactory
{
    static IFFAudioMixing* createInstance();
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* FFAudioMixing_hpp */
