#ifndef RECO_HPP
#define RECO_HPP

#include <iostream>
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "cudaSift.h"

#define RECO_W_OFFSET 80
#define RECO_H_OFFSET 20

union charint {
    char b[4];
    int i;
};

union charfloat {
    char b[4];
    float f;
};

struct frameBuffer {
    int frmID;
    int dataType;
    int bufferSize;
    char* buffer;
};

struct resBuffer {
    charint resID;
    charint resType;
    charint markerNum;
    char* buffer;
};

struct recognizedMarker {
    charint markerID;
    charint height, width;
    cv::Point2f corners[4];
    std::string markername;
};

double wallclock();
int parseCMD(char *argv[]);
void loadImages(std::vector<char *> onlineImages); 
void trainParams(); 
void loadParams();
void encodeDatabase();
void test();
bool query(cv::Mat queryImage, recognizedMarker &marker);
void freeParams();
#endif
