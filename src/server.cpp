#include "reco.hpp"
#include "cuda_files.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <iomanip>
#include <queue>
#include <fstream>

#define MESSAGE_ECHO 0
#define FEATURES 1
#define IMAGE_DETECT 2
#define BOUNDARY 3
#define PACKET_SIZE 60000
#define RES_SIZE 512
//#define TRAIN

using namespace std;
using namespace cv;

struct sockaddr_in localAddr;
struct sockaddr_in remoteAddr;
socklen_t addrlen = sizeof(remoteAddr);

queue<frameBuffer> frames;
queue<resBuffer> results;
int recognizedMarkerID;

vector<char *> onlineImages;
vector<char *> onlineAnnotations;

void *ThreadReceiverFunction(void *socket) {
    cout<<"Receiver Thread Created!"<<endl;
    char tmp[4];
    char buffer[PACKET_SIZE];
    int sock = *((int*)socket);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        recvfrom(sock, buffer, PACKET_SIZE, 0, (struct sockaddr *)&remoteAddr, &addrlen);

        frameBuffer curFrame;    
        memcpy(tmp, buffer, 4);
        curFrame.frmID = *(int*)tmp;        
        memcpy(tmp, &(buffer[4]), 4);
        curFrame.dataType = *(int*)tmp;

        if(curFrame.dataType == MESSAGE_ECHO) {
            cout<<"echo message!"<<endl;
            charint echoID;
            echoID.i = curFrame.frmID;
            char echo[4];
            memcpy(echo, echoID.b, 4);
            sendto(sock, echo, sizeof(echo), 0, (struct sockaddr *)&remoteAddr, addrlen);
            cout<<"echo reply sent!"<<endl;
            continue;
        }

        memcpy(tmp, &(buffer[8]), 4);
        curFrame.bufferSize = *(int*)tmp;
        cout<<"frame "<<curFrame.frmID<<" received, filesize: "<<curFrame.bufferSize;
        cout<<" at "<<setprecision(15)<<wallclock()<<endl;
        curFrame.buffer = new char[curFrame.bufferSize];
        memset(curFrame.buffer, 0, curFrame.bufferSize);
        memcpy(curFrame.buffer, &(buffer[12]), curFrame.bufferSize);
        
        frames.push(curFrame);
    }
}

void *ThreadSenderFunction(void *socket) {
    cout << "Sender Thread Created!" << endl;
    char buffer[RES_SIZE];
    int sock = *((int*)socket);

    while (1) {
        if(results.empty()) {
            this_thread::sleep_for(chrono::milliseconds(1));
            continue;
        }

        resBuffer curRes = results.front();
        results.pop();

        memset(buffer, 0, sizeof(buffer));
        memcpy(buffer, curRes.resID.b, 4);
        memcpy(&(buffer[4]), curRes.resType.b, 4);
        memcpy(&(buffer[8]), curRes.markerNum.b, 4);
        if(curRes.markerNum.i != 0)
            memcpy(&(buffer[12]), curRes.buffer, 100 * curRes.markerNum.i);
        sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&remoteAddr, addrlen);
        cout<<"frame "<<curRes.resID.i<<" res sent, "<<"marker#: "<<curRes.markerNum.i;
        cout<<" at "<<setprecision(15)<<wallclock()<<endl<<endl;
    }    
}

void *ThreadProcessFunction(void *param) {
    cout<<"Process Thread Created!"<<endl;
    recognizedMarker marker;
    bool markerDetected = false;

    while (1) {
        if(frames.empty()) {
            this_thread::sleep_for(chrono::milliseconds(1));
            continue;
        }

        frameBuffer curFrame = frames.front();
        frames.pop();

        int frmID = curFrame.frmID;
        int frmDataType = curFrame.dataType;
        int frmSize = curFrame.bufferSize;
        char* frmdata = curFrame.buffer;
        
        if(frmDataType == IMAGE_DETECT) {
            vector<uchar> imgdata(frmdata, frmdata + frmSize);
            Mat img_scene = imdecode(imgdata, CV_LOAD_IMAGE_GRAYSCALE);
            Mat detect = img_scene(Rect(RECO_W_OFFSET, RECO_H_OFFSET, 800, 500));
            markerDetected = query(detect, marker);
        }

        resBuffer curRes;
        if(markerDetected) {
            charfloat p;
            curRes.resID.i = frmID;
            curRes.resType.i = BOUNDARY;
            curRes.markerNum.i = 1;
            curRes.buffer = new char[100 * curRes.markerNum.i];

            int pointer = 0;
            memcpy(&(curRes.buffer[pointer]), marker.markerID.b, 4);
            pointer += 4;
            memcpy(&(curRes.buffer[pointer]), marker.height.b, 4);
            pointer += 4;
            memcpy(&(curRes.buffer[pointer]), marker.width.b, 4);
            pointer += 4;

            for(int j = 0; j < 4; j++) {
                p.f = marker.corners[j].x;
                memcpy(&(curRes.buffer[pointer]), p.b, 4);
                pointer+=4;
                p.f = marker.corners[j].y;
                memcpy(&(curRes.buffer[pointer]), p.b, 4);        
                pointer+=4;            
            }

            memcpy(&(curRes.buffer[pointer]), marker.markername.data(), marker.markername.length());

            recognizedMarkerID = marker.markerID.i;
        }
        else {
            curRes.resID.i = frmID;
            curRes.markerNum.i = 0;
        }

        results.push(curRes);
    }
}

void *ThreadAnnotationFunction(void *socket) {
    cout<<"Annotation Thread Created!"<<endl;
    int sockfd = *((int*)socket);
    int newsockfd;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int n; 
    
    while(1) {
        listen(sockfd,5);
        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) 
            cout<<"ERROR on accept"<<endl;

        char* annotation = 0;
        int length;
        const char *fileName;
        if(recognizedMarkerID < onlineAnnotations.size())
            fileName = onlineAnnotations[recognizedMarkerID];
        else
            fileName = "data/annotation/default.mp4";
        FILE* file = fopen(fileName, "rb");

        if(file) {
            fseek(file, 0, SEEK_END);
            length = ftell(file);
            cout<<"annotation file size: "<<length<<endl;
            fseek(file, 0, SEEK_SET);
            annotation = (char*)malloc(length);
            if(annotation) n = fread(annotation, 1, length, file);
            fclose(file);
        }
        n = write(newsockfd,annotation,length);
        cout<<"annotation sent"<<endl;
        delete[] annotation;
        if (n < 0) cout<<"ERROR writing to socket"<<endl;
        close(newsockfd);
    }
}

void runServer(int port) {
    pthread_t senderThread, receiverThread, processThread, annotationThread;
    int ret1, ret2, ret3, ret4;
    char buffer[PACKET_SIZE];
    char fileid[4];
    int status = 0;
    int sockTCP, sockUDP;

    memset((char*)&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(port);

#ifdef ANNOTATION
    if ((sockTCP = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout<<"ERROR opening tcp socket"<<endl;
        exit(1);
    }
    if (bind(sockTCP, (struct sockaddr *) &localAddr, sizeof(localAddr)) < 0) {
        cout<<"ERROR on tcp binding"<<endl;
        exit(1);
    }
#endif
    if((sockUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cout<<"ERROR opening udp socket"<<endl;
        exit(1);
    }
    if(bind(sockUDP, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
        cout<<"ERROR on udp binding"<<endl;
        exit(1);
    }
    cout << endl << "========server started, waiting for clients==========" << endl;

    ret1 = pthread_create(&receiverThread, NULL, ThreadReceiverFunction, (void *)&sockUDP);
    ret2 = pthread_create(&processThread, NULL, ThreadProcessFunction, NULL);
    ret3 = pthread_create(&senderThread, NULL, ThreadSenderFunction, (void *)&sockUDP);
#ifdef ANNOTATION
    ret3 = pthread_create(&annotationThread, NULL, ThreadAnnotationFunction, (void *)&sockTCP);
#endif

    pthread_join(receiverThread, NULL);
    pthread_join(processThread, NULL);
    pthread_join(senderThread, NULL);
#ifdef ANNOTATION
    pthread_join(annotationThread, NULL);
#endif
}

void loadOnline() 
{
    ifstream file("data/onlineData.dat");
    string line;
    int i = 0;
    while(getline(file, line)) {
        char* fileName = new char[256];
        strcpy(fileName, line.c_str());

        if(i%2 == 0) onlineImages.push_back(fileName);
        else onlineAnnotations.push_back(fileName);
        ++i;
    }
    file.close();
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        cout << "Usage: " << argv[0] << " size[s/m/l] NN#[1/2/3/4/5] port" << endl;
        return 1;
    }

    int port = parseCMD(argv);
    loadOnline();
    loadImages(onlineImages);
#ifdef TRAIN
    trainParams();
#else
    loadParams();
#endif
    encodeDatabase(); 
    test();
    runServer(port);

    freeParams();
    return 0;
}
