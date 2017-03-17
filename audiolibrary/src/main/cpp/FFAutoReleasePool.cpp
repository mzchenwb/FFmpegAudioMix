//
//  FFAutoReleasePool.cpp
//  FFAudioMixing
//
//  Created by fanzhang on 16/3/27.
//  Copyright © 2016年 bbo. All rights reserved.
//

#include "FFAutoReleasePool.hpp"

void FFAutoReleasePool::autoRelease(FFAutoReleaseFunc func)
{
    _releaseStack.push(func);
}

FFAutoReleasePool::~FFAutoReleasePool()
{
    while (!_releaseStack.empty())
    {
        FFAutoReleaseFunc& func = _releaseStack.top();
        func();
        _releaseStack.pop();
    }
}