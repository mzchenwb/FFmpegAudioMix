package com.chenwb.audiolibrary;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Process;
import android.util.Log;

import java.io.IOException;

public class FFRecorder {
    private static final String TAG = FFRecorder.class.getSimpleName();
    private static final int FRAME_COUNT = 1024 * 2;
    private static final int SAMPLE_RATE_IN_HZ = 44100;
    private static final PCMFormat AUDIO_FORMAT = PCMFormat.PCM_16BIT;

    private AudioRecord audioRecord = null;

    private int bufferSize;
    private byte[] buffer;

    private ScheduleThread mScheduleThread;

    private boolean isRecording = false;
    private int mCurrAmplitude;
    private Runnable mEndedCallBack;
    private String mCurrOutputPath;

    public void startRecording(String outputPath, final Runnable startCallBack) throws IOException {
        if (isRecording) return;
        printLog("Start recording");
        printLog("BufferSize = " + bufferSize);

        // Initialize audioRecord if it's null.
        if (audioRecord == null) {
            initAudioRecorder(outputPath);
            mCurrOutputPath = outputPath;
        }
        audioRecord.startRecording();
        isRecording = true;

        new Thread("RecordThread") {
            @Override
            public void run() {
                Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
                mScheduleThread.waitForStart();

                if (startCallBack != null) {
                    startCallBack.run();
                }
                long spendTime = System.currentTimeMillis();
                long mDuration = 0;

                int count = 0;
                int maxSamples = SAMPLE_RATE_IN_HZ / 1000 * 2 * 100; //每100毫秒计算一次音量值
                long volume = 0;
                while (isRecording) {
                    int bytes = audioRecord.read(buffer, 0, bufferSize);
                    if (bytes > 0) {
                        try {
                            mScheduleThread.writeData(buffer, bytes);

                            //calculate the RMS (Root-Mean-Square).
                            for (int i = 0; i < bytes - 1; i += 2) {
                                int sampleValue = (buffer[i + 1] << 8) | buffer[i];
                                volume += sampleValue * sampleValue;
                                count += 1;
                                if (count >= maxSamples) {
                                    mCurrAmplitude = (int) Math.sqrt(volume / maxSamples);
                                    count = 0;
                                    volume = 0;
                                }
                            }
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                    mDuration = System.currentTimeMillis() - spendTime;
                }

                // release and finalize audioRecord
                try {
                    audioRecord.stop();
                    audioRecord.release();
                    audioRecord = null;

                    // stop the encoding thread and try to wait
                    // until the thread finishes its job

                    mScheduleThread.prepareStop();

                    printLog("waiting for encoding thread");
                    mScheduleThread.join();
                    printLog("done encoding thread");

                } catch (Exception e) {
                    printLog("Faile to join encode thread");
                } finally {
                    if (mEndedCallBack != null)
                        mEndedCallBack.run();
                }

                isRecording = false;
                spendTime = System.currentTimeMillis() - spendTime;
                printLog("spendTime = " + getFormatTime(spendTime) + " duration = " + getFormatTime(mDuration));
            }
        }.start();
    }

    protected String getFormatTime(long mSeconds) {
        long seconds = mSeconds / 1000;
        int ss = (int) (seconds % 60);
        int mm = (int) (seconds / 60);

        return String.format("%02d:%02d", mm, ss);
    }

    private void printLog(String text) {
//        if (BuildConfig.DEBUG)
        Log.d(TAG, text);
    }

    private void initAudioRecorder(String outputPath) throws IOException {
        int bytesPerFrame = AUDIO_FORMAT.getBytesPerFrame();
        /* Get number of samples. Calculate the buffer size (round up to the
           factor of given frame size) */
        int channelConfig = AudioFormat.CHANNEL_IN_MONO;
        int minBufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE_IN_HZ, channelConfig,
                AUDIO_FORMAT.getAudioFormat());
        int frameSize = minBufferSize / bytesPerFrame;
        printLog("Frame size: ---------1---" + frameSize);
        if (frameSize % FRAME_COUNT != 0) {
            frameSize = frameSize + (FRAME_COUNT - frameSize % FRAME_COUNT);
            printLog("Frame size: ---------2---" + frameSize);
        }
        bufferSize = frameSize * bytesPerFrame * 2;

        audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC,
                SAMPLE_RATE_IN_HZ, channelConfig, AUDIO_FORMAT.getAudioFormat(),
                bufferSize);

        buffer = new byte[bufferSize];

        mScheduleThread = new ScheduleThread(outputPath, bufferSize);
        mScheduleThread.start();
    }

    public void stopRecording(Runnable endCallBack) {
        printLog("stop recording");
        isRecording = false;
        mEndedCallBack = endCallBack;
    }

    public int getCurAmplitude() {
        return mCurrAmplitude;
    }

    public boolean isRecording() {
        return isRecording;
    }

    public String getCurrOutputPath() {
        return mCurrOutputPath;
    }
}