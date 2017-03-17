package com.chenwb.audiolibrary;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.concurrent.CountDownLatch;

class ScheduleThread extends Thread {
//  private static final String TAG = ScheduleHandle.class.getName();

    private static final int PROCESS_DATA = 1;
    private static final int PROCESS_STOP = 2;

    private CountDownLatch mCountDown = new CountDownLatch(1);
    private ScheduleHandle scheduleHandle;

    private ByteArrayOutputStream mOutput1;
    private ByteArrayOutputStream mOutput2;
    private boolean isWriteToOut1 = true;

    ScheduleThread(String outputPath, int tempSize) throws IOException {
        int ret = FFBufferEncoder.startEncode(outputPath, ".mp4", 128000);
        if (ret != 0) {
            throw new IOException("startEncode error");
        }

        mOutput1 = new ByteArrayOutputStream(tempSize);
        mOutput2 = new ByteArrayOutputStream(tempSize);
    }

    private static class ScheduleHandle extends Handler {
        WeakReference<ScheduleThread> mReference;
        boolean isHandled = true;

        ScheduleHandle(ScheduleThread thread) {
            mReference = new WeakReference<>(thread);
        }

        @Override
        public void handleMessage(Message msg) {
            ScheduleThread thread = mReference.get();
            if (thread == null)
                return;

            isHandled = false;
            if (msg.what == PROCESS_DATA) {
                ByteArrayOutputStream output;
                if (thread.isWriteToOut1) {
                    output = thread.mOutput1;
                    thread.isWriteToOut1 = false;
                } else {
                    output = thread.mOutput2;
                    thread.isWriteToOut1 = true;
                }

                handleData(output);

            } else {
                if (thread.isWriteToOut1) {
                    handleData(thread.mOutput1);
                    handleData(thread.mOutput2);
                } else {
                    handleData(thread.mOutput2);
                    handleData(thread.mOutput1);
                }
                FFBufferEncoder.endInput();
                getLooper().quit();
            }
            isHandled = true;
        }

        private void handleData(ByteArrayOutputStream output) {
            if (output != null && output.size() > 0) {
                try {
//                    Log.d(TAG, "handleData: starting");
//                    long timeMillis = System.currentTimeMillis();
                    output.flush();
                    ByteArrayInputStream input = new ByteArrayInputStream(output.toByteArray());
                    byte[] bytes = new byte[2048];
                    int len;
                    while ((len = input.read(bytes, 0, 2048)) != -1) {
                        FFBufferEncoder.appendData(bytes, len);
                    }
//                    timeMillis = System.currentTimeMillis() - timeMillis;
//                    Log.d(TAG, "handleData: stop size = " + output.size() + " time = " + timeMillis);
                    output.reset();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    @Override
    public void run() {
        Looper.prepare();
        scheduleHandle = new ScheduleHandle(this);
        mCountDown.countDown();
        Looper.loop();
    }

    public void waitForStart() {
        try {
            mCountDown.await();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void writeData(byte[] bytes, int len) {
        if (isWriteToOut1) {
            mOutput1.write(bytes, 0, len);
        } else {
            mOutput2.write(bytes, 0, len);
        }

        if (scheduleHandle.isHandled)
            scheduleHandle.sendEmptyMessage(PROCESS_DATA);
    }

    public void prepareStop() {
        scheduleHandle.sendEmptyMessage(PROCESS_STOP);
    }
}
