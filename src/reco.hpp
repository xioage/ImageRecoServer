#ifndef RECO_HPP
#define RECO_HPP

#include <iostream>
#include <vector>
#include "cudaSift.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

struct posterImg {
        std::string imgID;
        int rows, cols;
        std::vector<cv::KeyPoint> keypoints;
        cv::Mat descriptors;
};

struct frameBuffer {
        int frmID;
        int dataType;
        int bufferSize;
        char* buffer;
        void* nextFrameBuffer;
};

union charint
{
        char b[4];
        int i;
};

union charfloat
{
        char b[4];
        float f;
};

struct resBuffer {
        charint resID;
        charint resType;
        charint markerNum;
        char* buffer;
        void* nextResBuffer;
};

struct recognizedMarker {
        charint markerID;
        charint height, width;
        cv::Point2f corners[4];
        std::string markername;
};

void parseCMD(char *argv[]);
void loadImages(); 
//void trainParams(); 
void loadParams();
void encodeDatabase();
void test();
bool query(cv::Mat image, recognizedMarker &marker);
void freeParams();
#endif
