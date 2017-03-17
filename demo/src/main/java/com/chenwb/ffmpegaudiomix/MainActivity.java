package com.chenwb.ffmpegaudiomix;

import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;


import com.chenwb.audiolibrary.FFAudioMixing;
import com.chenwb.audiolibrary.FFBufferEncoder;
import com.chenwb.audiolibrary.FFRecorder;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

public class MainActivity extends AppCompatActivity implements Runnable, View.OnClickListener {

    private static final String TAG = MainActivity.class.getName();
    private FFRecorder mRecord;
    private TextView mText;
    private MediaPlayer mPlayer;
    private LinearLayout mPlayList;

    private FFAudioMixing mAudioMixing;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mText = (TextView) findViewById(R.id.sample_text);

        findViewById(R.id.record).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startRecord();
            }
        });

        findViewById(R.id.stop).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                stopRecord();
            }
        });

        mPlayList = (LinearLayout) findViewById(R.id.play);

        findViewById(R.id.merge).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mergeAudios();
            }
        });

        findViewById(R.id.clear).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                clearAudioCache();
            }
        });
    }

    private void clearAudioCache() {
        mPlayList.removeAllViews();
        File[] files = getExternalCacheDir().listFiles();
        if (files != null && files.length > 0) {
            for (File file : files) {
                file.delete();
            }
        }
    }

    private void mergeAudios() {
        if (mAudioMixing == null) {
            mAudioMixing = new FFAudioMixing();
        }
        int childCount = mPlayList.getChildCount();
        String[] audios = new String[childCount];
        for (int i = childCount - 1; i >= 0; i--) {
            View child = mPlayList.getChildAt(i);
            String filePath = (String) child.getTag();
            audios[childCount - 1 - i] = filePath;
        }

        File outputPath = new File(getExternalCacheDir(), "merged.mp3");
        FFAudioMixing.RecordAudio audio = new FFAudioMixing.RecordAudio(audios, outputPath.getAbsolutePath());

        mAudioMixing.startAudioMixing(audio);
    }

    private void concatAudios() {
        if (mAudioMixing == null) {
            mAudioMixing = new FFAudioMixing();
        }
        int childCount = mPlayList.getChildCount();
        String[] audios = new String[childCount];
        for (int i = childCount - 1; i >= 0; i--) {
            View child = mPlayList.getChildAt(i);
            String filePath = (String) child.getTag();
            audios[childCount - 1 - i] = filePath;
        }

        File outputPath = new File(getExternalCacheDir(), "merged.m4a");

        mAudioMixing.concatAudios(audios, outputPath.getAbsolutePath(), 2, true);
    }

    private void startRecord() {
        if (mRecord == null) {
            mRecord = new FFRecorder();
        }

        File dir = new File(getExternalCacheDir(), String.format("%d.m4a", System.currentTimeMillis()));

        try {
            mRecord.startRecording(dir.getAbsolutePath(), new Runnable() {
                @Override
                public void run() {
                    mText.post(MainActivity.this);

                }
            });
        } catch (IOException e) {
            e.printStackTrace();
        }

        if (mPlayer != null && mPlayer.isPlaying())
            mPlayer.stop();
    }

    private void startLoudNormAudio() {
        File dir = new File(Environment.getExternalStorageDirectory(), "test.m4a");
        File outputPath = new File(Environment.getExternalStorageDirectory(), "test_out2.m4a");

        FFAudioMixing ffAudioMixing = new FFAudioMixing();
        ffAudioMixing.loudnormAudio(dir.getAbsolutePath(), outputPath.getAbsolutePath());
    }

    private void appendData() {
        Log.d(TAG, "appendData: start");
        File dir = new File(Environment.getExternalStorageDirectory(), "test_out2.pcm");
        File outputPath = new File(Environment.getExternalStorageDirectory(), "test_out2.m4a");

        try {
            FileInputStream fileInputStream = new FileInputStream(dir);
            int fileLength = (int) dir.length();
            byte[] buf = new byte[fileLength];
            int readSize;
            FFBufferEncoder.startEncode(outputPath.getAbsolutePath(), ".mp3", 128000);
            while ((readSize = fileInputStream.read(buf, 0, fileLength)) != -1) {
                long timeMillis = System.currentTimeMillis();
                FFBufferEncoder.appendData(buf, readSize);
                Log.d(TAG, "appendData: end time = " + (System.currentTimeMillis() - timeMillis));
            }

            FFBufferEncoder.endInput();

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void stopRecord() {
        mRecord.stopRecording(new Runnable() {
            @Override
            public void run() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        addPlayBtn(mRecord.getCurrOutputPath());
                    }
                });
            }
        });
    }

    private void addPlayBtn(String filePath) {
        Button btn = new Button(this);
        File file = new File(filePath);
        btn.setTag(filePath);
        btn.setText(file.getName());
        btn.setOnClickListener(this);
        mPlayList.addView(btn, 0);
    }

    private void play(String filePath) {
        File dir = new File(filePath);
        if (!dir.exists())
            return;

        if (mPlayer == null) {
            mPlayer = new MediaPlayer();
        }
        try {
            mPlayer.reset();
            mPlayer.setDataSource(dir.getAbsolutePath());
            mPlayer.prepare();
            mPlayer.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void run() {
        if (mRecord.isRecording()) {
            mText.setText("CurAmplitude = " + mRecord.getCurAmplitude());
            mText.postDelayed(this, 200);
        }
    }

    @Override
    public void onClick(View v) {
        String filePath = (String) v.getTag();
        play(filePath);
    }

}
