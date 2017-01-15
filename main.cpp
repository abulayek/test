/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: quangthai
 *
 * Created on August 3, 2016, 5:16 PM
 */
#include <math.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
}

#include <cstdlib>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/highgui.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/imgproc.hpp>
#include <bits/stl_vector.h>
#include <libavutil/pixfmt.h>
#include <opencv2/core/mat.hpp>
#include <libavutil/rational.h>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>
#include <string.h>
#include <bits/basic_string.h>
#include "encode_video.h"


using namespace std;
using namespace cv;

vector<Mat> getFrames(string path, int& refFrameIndex);
vector<Mat> reconstructFromPosAndNeg(vector<Mat>& posFrames, vector<Mat>& negFrames, int refFrameIndex);
vector<Mat> reconstructFromZig(vector<Mat>& frames, int refFrameIndex);
vector<Mat> reconstructFromAll(vector<Mat>& frames, int refFrameIndex);
//void writeVideo(string path, vector<Mat>& frames, int encoder, int fps, Size frameSize);
void writeVideo(string path, vector<Mat>& frames, float quality, int fps, int refFrameIndex = -1);

#define ENCODE_CODEC AV_CODEC_ID_MPEG4

/*
 * 
 */
int main() {
    avcodec_register_all();
    //    video_encode_example("test.mp4", AV_CODEC_ID_H264);

    string destDir = "/home/layek/CompressVideo/OutVideos/";
    string container = "mp4";
    int targetFPS = 5;
    Size frameSize(320, 240);
    float quality = 0.0; //from (0...100%)
    int refFrameIndex = 5;

    //Encode
    {
        VideoCapture vcap("/home/layek/CompressVideo/OriginalTestVideos/video.mp4");
        //        VideoCapture vcap("/home/quangthai/Desktop/Workspace/Projects/Resources/Car Traffic - YouTube.avi");

        int origFPS = vcap.get(CAP_PROP_FPS);
        int skipFrames = ceil((float) origFPS / targetFPS);
        vector<Mat> frames;
        while (true) {
            bool videoEnded = false;
            for (int i = 0; i < skipFrames; i++) {
                if (!vcap.grab()) {
                    videoEnded = true;
                    break;
                }
            }
            if (videoEnded) {
                break;
            }

            Mat frame;
            vcap>>frame;
            if (frame.empty()) {
                break;
            }
            resize(frame, frame, frameSize);
            frames.push_back(frame);
        }

        Mat& refFrame = frames[refFrameIndex];
        Mat blankFrame = Mat::zeros(refFrame.rows, refFrame.cols, CV_8UC3);
        vector<Mat> listPosFrame;
        vector<Mat> listNegFrame;

        for (int i = 0; i < frames.size(); i++) {
            Mat& frame = frames[i];
            Mat posFrame;
            Mat negFrame;
            if (i == refFrameIndex) {
                posFrame = refFrame;
                negFrame = blankFrame;
            } else {
                posFrame = frame - refFrame;
                negFrame = refFrame - frame;
            }

            listPosFrame.push_back(posFrame);
            listNegFrame.push_back(negFrame);
        }

        vector<Mat> listZigZag;
        vector<Mat> listAllInTurn;
        for (int i = 0; i < listPosFrame.size(); i++) {
            listZigZag.push_back(listPosFrame[i]);
            listZigZag.push_back(listNegFrame[i]);
        }
        for (int i = 0; i < listPosFrame.size(); i++) {
            listAllInTurn.push_back(listPosFrame[i]);
        }
        for (int i = 0; i < listNegFrame.size(); i++) {
            listAllInTurn.push_back(listNegFrame[i]);
        }

        writeVideo(destDir + "Negative." + container, listNegFrame, quality, targetFPS, -1);
        writeVideo(destDir + "Positive." + container, listPosFrame, quality, targetFPS, refFrameIndex);
        writeVideo(destDir + "ZigZag." + container, listZigZag, quality, targetFPS, refFrameIndex * 2);
        writeVideo(destDir + "AllInTurn." + container, listAllInTurn, quality, targetFPS, refFrameIndex);
    }

    //Decode
    {
        int refNegFrame, refPosFrame, refZigFrame, refAllFrame;
        vector<Mat> negFrames = getFrames(destDir + "Negative." + container, refNegFrame);
        vector<Mat> posFrames = getFrames(destDir + "Positive." + container, refPosFrame);
        vector<Mat> zigFrames = getFrames(destDir + "ZigZag." + container, refZigFrame);
        vector<Mat> allFrames = getFrames(destDir + "AllInTurn." + container, refAllFrame);

        vector<Mat> resPosNegFrames = reconstructFromPosAndNeg(posFrames, negFrames, refPosFrame);
        vector<Mat> resZigFrames = reconstructFromZig(zigFrames, refZigFrame);
        vector<Mat> resAllFrames = reconstructFromAll(allFrames, refAllFrame);

        writeVideo(destDir + "ResPosNeg." + container, resPosNegFrames, 100, targetFPS);
        writeVideo(destDir + "ResZigZag." + container, resZigFrames, 100, targetFPS);
        writeVideo(destDir + "ResAllInTurn." + container, resAllFrames, 100, targetFPS);
    }


    return 0;
}

vector<Mat> getFrames(string path, int& refFrameIndex) {
    vector<array<string, 2 >> metadata;
    read_video_description(path.c_str(), metadata);
    refFrameIndex = -1;
    for (int i = 0; i < metadata.size(); i++) {
        array<string, 2>& pair = metadata[i];
        if (strcmp(pair[0].c_str(), "title") == 0) {
            refFrameIndex = (int)(pair[1][0]);
        }
    }
    VideoCapture vcap(path);
    vector<Mat> frames;
    while (true) {
        Mat frame;
        vcap >> frame;
        if (frame.empty()) {
            break;
        }
        frames.push_back(frame);
    }
    return frames;
}

vector<Mat> reconstructFromPosAndNeg(vector<Mat>& posFrames, vector<Mat>& negFrames, int refFrameIndex) {
    if (posFrames.size() != negFrames.size()) {
        printf("reconstructFromPosAndNeg: number of frames combined is ODD (%ld + %ld)! Cannot separate!\n",
                posFrames.size(), negFrames.size());
        exit(0);
    }
    Mat& refFrame = posFrames[refFrameIndex];
    vector<Mat> resFrames;
    for (int i = 0; i < posFrames.size(); i++) {
        if (i == refFrameIndex) {
            resFrames.push_back(refFrame);
        } else {
            Mat resFrame = refFrame + posFrames[i] - negFrames[i];
            resFrames.push_back(resFrame);
        }
    }
    return resFrames;
}

vector<Mat> reconstructFromZig(vector<Mat>& frames, int refFrameIndex) {
    if (frames.size() % 2 != 0) {
        printf("reconstructFromZig: number of frames combined is ODD (%ld)! Cannot separate!\n",
                frames.size());
        frames.erase(frames.end());
        //        exit(0);
    }
    if (refFrameIndex % 2 != 0) {
        printf("reconstructFromZig: Reference frame Index must be EVEN!\n");
        exit(1);
    }
    vector<Mat> posFrames;
    vector<Mat> negFrames;
    for (int i = 0; i < frames.size(); i++) {
        if (i % 2 == 0) {
            posFrames.push_back(frames[i]);
        } else {
            negFrames.push_back(frames[i]);
        }
    }
    return reconstructFromPosAndNeg(posFrames, negFrames, refFrameIndex / 2);
}

vector<Mat> reconstructFromAll(vector<Mat>& frames, int refFrameIndex) {
    if (frames.size() % 2 != 0) {
        printf("reconstructFromAll: number of frames combined is ODD (%ld)! Cannot separate!\n",
                frames.size());
        frames.erase(frames.end());
        //        exit(0);
    }
    vector<Mat> posFrames;
    vector<Mat> negFrames;
    for (int i = 0; i < frames.size() / 2; i++) {
        posFrames.push_back(frames[i]);
    }
    for (int i = frames.size() / 2; i < frames.size(); i++) {
        negFrames.push_back(frames[i]);
    }
    return reconstructFromPosAndNeg(posFrames, negFrames, refFrameIndex);
}

void writeVideo(string path, vector<Mat>& frames, float quality, int fps, int refFrameIndex) {
    encode_video(path.c_str(), frames, quality, fps, refFrameIndex, ENCODE_CODEC);
}