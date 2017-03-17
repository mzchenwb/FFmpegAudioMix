//
//  FFAutoReleasePool.hpp
//  FFAudioMixing
//
//  Created by fanzhang on 16/3/27.
//  Copyright © 2016年 bbo. All rights reserved.
//

#ifndef FFAutoReleasePool_hpp
#define FFAutoReleasePool_hpp

#include <functional>
#include <stack>

typedef std::function<void ()> FFAutoReleaseFunc;

class FFAutoReleasePool
{
private:
    std::stack<FFAutoReleaseFunc> _releaseStack;
    
public:
    virtual ~FFAutoReleasePool();
    
public:
    void autoRelease(FFAutoReleaseFunc func);
};

#endif /* FFAutoReleasePool_hpp */
