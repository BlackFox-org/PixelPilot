//
// Created by Constantin on 1/9/2019.
//

#ifndef FPV_VR_VIDEOPLAYERN_H
#define FPV_VR_VIDEOPLAYERN_H

#include <jni.h>
#include "VideoDecoder.h"
#include "UdpReceiver.h"
#include "parser/H26XParser.h"
#include "minimp4.h"
#include <stdio.h>
#include <fcntl.h>
#include <fstream>
#include <queue>

class VideoPlayer{

public:
    VideoPlayer(JNIEnv * env, jobject context);
    enum VIDEO_DATA_TYPE{RTP_H264,RAW_H264,RTP_H265,RAW_H265};
    void onNewVideoData(const uint8_t* data,const std::size_t data_length,const VIDEO_DATA_TYPE videoDataType);
    /*
     * Set the surface the decoder can be configured with. When @param surface==nullptr
     * It is guaranteed that the surface is not used by the decoder anymore when this call returns
     */
    void setVideoSurface(JNIEnv* env, jobject surface);
    /*
     * Start the receiver and ground recorder if enabled
     */
    void start(JNIEnv *env,jobject androidContext, jstring codec);
    /**
     * Stop the receiver and ground recorder if enabled
     */
    void stop(JNIEnv *env,jobject androidContext);
    /*
     * Returns a string with the current configuration for debugging
     */
    std::string getInfoString()const;

    void startDvr(JNIEnv *env,jint fd);
    void stopDvr();

private:
    void onNewNALU(const NALU& nalu);
    //Assumptions: Max bitrate: 40 MBit/s, Max time to buffer: 100ms
    //5 MB should be plenty !
    static constexpr const size_t WANTED_UDP_RCVBUF_SIZE=1024*1024*5;
    // Retrieve settings from shared preferences
    enum SOURCE_TYPE_OPTIONS{UDP,FILE,ASSETS,VIA_FFMPEG_URL,EXTERNAL};
    const std::string GROUND_RECORDING_DIRECTORY;
    JavaVM* javaVm=nullptr;
    H26XParser mParser;

    // DVR attributes
    int dvr_fd;
    std::queue<NALU> naluQueue;
    std::mutex mtx;
    std::condition_variable cv;
    bool stopFlag = false;
    std::thread processingThread;

    void enqueueNALU(const NALU& nalu) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            naluQueue.push(nalu);
        }
        cv.notify_one();
    }

    void startProcessing() {
        stopFlag = false;
        processingThread = std::thread(&VideoPlayer::processQueue, this);
    }

    void stopProcessing() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stopFlag = true;
        }
        cv.notify_all();
        if (processingThread.joinable()) {
            processingThread.join();
        }
    }

    void processQueue();

public:
    VideoDecoder videoDecoder;
    std::unique_ptr<UDPReceiver> mUDPReceiver;
    long nNALUsAtLastCall=0;

public:
    DecodingInfo latestDecodingInfo{};
    std::atomic<bool> latestDecodingInfoChanged=false;
    VideoRatio latestVideoRatio{};
    std::atomic<bool> latestVideoRatioChanged=false;

    bool lastFrameWasAUD=false;
};

#endif //FPV_VR_VIDEOPLAYERN_H
