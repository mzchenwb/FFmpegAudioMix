package com.chenwb.audiolibrary;

public class FFBufferEncoder {
    static {
        System.loadLibrary("avcodec-57");
        System.loadLibrary("avfilter-6");
        System.loadLibrary("avformat-57");
        System.loadLibrary("avutil-55");
        System.loadLibrary("ebur128");
        System.loadLibrary("mp3lame");
        System.loadLibrary("postproc-54");
        System.loadLibrary("swresample-2");
        System.loadLibrary("swscale-4");
        System.loadLibrary("audiomixing");
    }

    public native static int startEncode(String outFilePath, String outFileTyp, int outBitRate);

    public native static int appendData(byte[] data, int len);

    public native static int endInput();
}
