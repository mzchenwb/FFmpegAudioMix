#include <jni.h>
#include <string>


extern "C"
{
#include <libavcodec/avcodec.h>
jstring
Java_com_chenweibin_testffmpeg_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    char info[10000] = {0};
    sprintf(info, "%s\n", avcodec_configuration());
    return env->NewStringUTF(info);
}
}

