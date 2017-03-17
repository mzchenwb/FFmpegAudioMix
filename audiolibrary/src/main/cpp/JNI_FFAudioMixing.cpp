
#include <jni.h>
#include <sstream>
#include "FFAudioMixing.hpp"

extern "C"
{
JNIEXPORT jstring JNICALL
Java_com_chenwb_audiolibrary_FFAudioMixing_startAudioMixing(JNIEnv *env, jobject instance,
                                                                 jstring jBeginEffect,
                                                                 jstring jEndEffect_,
                                                                 jboolean jHaveIntroPage,
                                                                 jboolean jHaveEndingPage,
                                                                 jobjectArray jVoicePages,
                                                                 jdouble jTimeSpanSec,
                                                                 jstring jBkgMusicFile,
                                                                 jdouble jBkgVolume,
                                                                 jstring jOutputFile,
                                                                 jboolean isM4A) {

    //找到java中的类
    jclass cls = env->FindClass("com/chenwb/audiolibrary/FFAudioMixing");
    //再找类中的方法
    jmethodID printMessage = env->GetMethodID(cls, "printMessage", "(Ljava/lang/String;)V");
    if (printMessage == NULL) {
        return env->NewStringUTF("init error");
    }

    jint size = env->GetArrayLength(jVoicePages);
    if (size <= 0) {
        return env->NewStringUTF("File size Empty!");
    }

    env->CallVoidMethod(instance, printMessage, env->NewStringUTF("Start ++++++"));

    const char *beginEffect = env->GetStringUTFChars(jBeginEffect, JNI_FALSE);
    const char *endEffect = env->GetStringUTFChars(jEndEffect_, JNI_FALSE);
    const char *bkgMusicFile = env->GetStringUTFChars(jBkgMusicFile, JNI_FALSE);
    const char *outputFile = env->GetStringUTFChars(jOutputFile, JNI_FALSE);

    std::vector<std::string> inputFiles;
    for (jint i = 0; i < size; i++) {
        jstring strObj = (jstring) env->GetObjectArrayElement(jVoicePages, i);
        const char *chr = env->GetStringUTFChars(strObj, JNI_FALSE);
        inputFiles.push_back(chr);
        env->ReleaseStringUTFChars(strObj, chr);
    }
    std::string beginEffectFile(beginEffect);
    std::string endEffectFile(endEffect);
    std::string backgroundFile(bkgMusicFile);
    std::string outputFileStr(outputFile);

    env->ReleaseStringUTFChars(jBeginEffect, beginEffect);
    env->ReleaseStringUTFChars(jEndEffect_, endEffect);
    env->ReleaseStringUTFChars(jBkgMusicFile, bkgMusicFile);
    env->ReleaseStringUTFChars(jOutputFile, outputFile);

    IFFAudioMixing* audioMixing = FFAudioMixingFactory::createInstance();

    if (isM4A) {
        audioMixing->init(".mp4", 128000);
    } else {
        audioMixing->init();
    }

    env->CallVoidMethod(instance, printMessage, env->NewStringUTF("combineAudios starting ++++++"));
    int err = audioMixing->combineAudios(beginEffectFile,
                                        endEffectFile,
                                        jHaveIntroPage,
                                        jHaveEndingPage,
                                        inputFiles,
                                        jTimeSpanSec,
                                        backgroundFile,
                                        jBkgVolume,
                                        outputFileStr);

    audioMixing->destroy();

    env->CallVoidMethod(instance, printMessage, env->NewStringUTF("combineAudios end ------- "));

    char str[15];
    sprintf(str, "result = %d\n", err);
    return env->NewStringUTF(str);
}

JNIEXPORT jstring JNICALL
Java_com_chenwb_audiolibrary_FFAudioMixing_loudnormAudio(JNIEnv *env, jobject instance,
                                                              jstring inputFile_,
                                                              jstring outputFile_) {
    const char *inputFile = env->GetStringUTFChars(inputFile_, 0);
    const char *outputFile = env->GetStringUTFChars(outputFile_, 0);

    std::string inputFileStr(inputFile);
    std::string outputFileStr(outputFile);

    IFFAudioMixing* audioMixing = FFAudioMixingFactory::createInstance();
    audioMixing->init(".mp4", 128000);

    int err = audioMixing->loudnormAudio(inputFileStr, outputFileStr);
    audioMixing->destroy();

    env->ReleaseStringUTFChars(inputFile_, inputFile);
    env->ReleaseStringUTFChars(outputFile_, outputFile);

    char str[15];
    sprintf(str, "result = %d\n", err);
    return env->NewStringUTF(str);
}

JNIEXPORT jstring JNICALL
Java_com_chenwb_audiolibrary_FFAudioMixing_concatAudios(JNIEnv *env, jobject instance,
                                                         jobjectArray audioFiles,
                                                         jdouble timeSpanSec,
                                                         jstring outputFile_, jboolean isM4a) {
    const char *outputFile = env->GetStringUTFChars(outputFile_, 0);

    jint size = env->GetArrayLength(audioFiles);
    std::vector<std::string> inputFiles;
    for (jint i = 0; i < size; i++) {
        jstring strObj = (jstring) env->GetObjectArrayElement(audioFiles, i);
        const char *chr = env->GetStringUTFChars(strObj, JNI_FALSE);
        inputFiles.push_back(chr);
        env->ReleaseStringUTFChars(strObj, chr);
    }

    IFFAudioMixing* audioMixing = FFAudioMixingFactory::createInstance();

    if (isM4a) {
        audioMixing->init(".mp4", 128000);
    } else {
        audioMixing->init();
    }

    int err = audioMixing->concatAudios(inputFiles, timeSpanSec, outputFile);

    audioMixing->destroy();

    env->ReleaseStringUTFChars(outputFile_, outputFile);

    char str[15];
    sprintf(str, "result = %d\n", err);
    return env->NewStringUTF(str);
}
}