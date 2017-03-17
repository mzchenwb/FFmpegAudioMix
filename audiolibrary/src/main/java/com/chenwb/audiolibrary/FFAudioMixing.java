package com.chenwb.audiolibrary;

public class FFAudioMixing {
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

    public String startAudioMixing(RecordAudio recordAudio) {
        return startAudioMixing(recordAudio.beginEffect,
                recordAudio.endEffect,
                recordAudio.haveIntroPage,
                recordAudio.haveEndingPage,
                recordAudio.voicePages,
                recordAudio.timeSpanSec,
                recordAudio.bkgMusicFile,
                recordAudio.bkgVolume,
                recordAudio.outputFile,
                recordAudio.isM4a);
    }

    public native String startAudioMixing(String beginEffect,
                                          String endEffect,
                                          boolean haveIntroPage,
                                          boolean haveEndingPage,
                                          String[] voicePages,
                                          double timeSpanSec,
                                          String bkgMusicFile,
                                          double bkgVolume,
                                          String outputFile,
                                          boolean isM4a);

    public native String loudnormAudio(String inputFile, String outputFile);

    public native String concatAudios(String[] audioFiles, String outputFile, double timeSpanSec, boolean isM4a);

    protected void printMessage(String message) {
    }

    public static class RecordAudio {
        public String beginEffect = "";
        public String endEffect = "";
        public boolean haveIntroPage = false;
        public boolean haveEndingPage = false;
        public String[] voicePages;
        public double timeSpanSec = 2;
        public String bkgMusicFile = "";
        public double bkgVolume = 0.2;
        public String outputFile;
        public boolean isM4a;

        public RecordAudio(String[] voicePages, String outputFile) {
            this.voicePages = voicePages;
            this.outputFile = outputFile;
        }
    }
}


