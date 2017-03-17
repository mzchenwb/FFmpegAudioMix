//
// Created by chen weibin on 16/9/18.
//
#include <string.h>
#include <jni.h>
#include <sstream>
#include "FFAudioBufferEncoder.hpp"
#include <android/log.h>

#define DEBUG 1

#define LOG(...) __android_log_print(ANDROID_LOG_ERROR,"FFAudioBufferEncoder",__VA_ARGS__)

extern "C" {

FFAudioBufferEncoder *glf_encoder = NULL;

JNIEXPORT jint JNICALL
Java_com_chenwb_audiolibrary_FFBufferEncoder_startEncode(JNIEnv *env, jobject instance,
                                                              jstring outFilePath_,
                                                              jstring outFileTyp_,
                                                              jint outBitRate) {
    const char *outFilePath = env->GetStringUTFChars(outFilePath_, 0);
    const char *outFileTyp = env->GetStringUTFChars(outFileTyp_, 0);

    glf_encoder = new FFAudioBufferEncoder(outFilePath, outFileTyp, outBitRate);
    int error = glf_encoder->beginInput();

    if (error != 0) {
        LOG("beginInput err %s", &FFAudioHelper::getErrorText(error).front());
    }

    env->ReleaseStringUTFChars(outFilePath_, outFilePath);
    env->ReleaseStringUTFChars(outFileTyp_, outFileTyp);

    return error;
}

JNIEXPORT jint JNICALL
Java_com_chenwb_audiolibrary_FFBufferEncoder_appendData(JNIEnv *env, jobject instance,
                                                             jbyteArray data_, jint len) {
    jbyte *data = env->GetByteArrayElements(data_, NULL);

    int error = glf_encoder->appendData((const uint8_t *) data, len);
    if (error != 0) {
        LOG("appendData err %s", &FFAudioHelper::getErrorText(error).front());
    }

    env->ReleaseByteArrayElements(data_, data, 0);

    return error;
}

JNIEXPORT jint JNICALL
Java_com_chenwb_audiolibrary_FFBufferEncoder_endInput(JNIEnv *env, jobject instance) {
    int error = 0;
    if (glf_encoder) {
        error = glf_encoder->endInput();
        if (error != 0) {
            LOG("endInput err %s", &FFAudioHelper::getErrorText(error).front());
        }
        delete glf_encoder;
        glf_encoder = NULL;
    }

    return error;
}
}