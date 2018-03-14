#include "reco.hpp"
#include "cuda_files.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <iomanip>

#define MESSAGE_ECHO 0
#define FEATURES 1
#define IMAGE_DETECT 2
#define BOUNDARY 3
#define PORT 51717
#define PACKET_SIZE 60000
#define RES_SIZE 512

using namespace std;
using namespace cv;

bool client_alive;
struct sockaddr_in localAddr;
struct sockaddr_in remoteAddr;
socklen_t addrlen = sizeof(remoteAddr);

struct frameBuffer* frameBufferHead = 0;
struct frameBuffer* frameBufferTail = 0;
pthread_mutex_t fbhMutex;
struct resBuffer* resBufferHead = 0;
struct resBuffer* resBufferTail = 0;
pthread_mutex_t rbhMutex;

void *ThreadReceiverFunction(void *socket) {
    cout<<"Receiver Thread Created!"<<endl;
    char tmp[4];
    char buffer[PACKET_SIZE];

    struct frameBuffer* curFrame;    

    int sock = *((int*)socket);

    pthread_mutex_lock(&fbhMutex);
    while (frameBufferHead != NULL) {
        delete(frameBufferHead->buffer);
        void* nextFrameBuffer = frameBufferHead->nextFrameBuffer;
        delete(frameBufferHead);
        frameBufferHead = (struct frameBuffer*)nextFrameBuffer;
    }
    pthread_mutex_unlock(&fbhMutex);

    while (1) {
        curFrame = new frameBuffer();
        curFrame->nextFrameBuffer = NULL;

        memset(buffer, 0, sizeof(buffer));
        recvfrom(sock, buffer, PACKET_SIZE, 0, (struct sockaddr *)&remoteAddr, &addrlen);

        memcpy(tmp, buffer, 4);
        curFrame->frmID = *(int*)tmp;        

        memcpy(tmp, &(buffer[4]), 4);
        curFrame->dataType = *(int*)tmp;
        if(curFrame->dataType == MESSAGE_ECHO) {
            cout<<"echo message!"<<endl;
            charint echoID;
            echoID.i = curFrame->frmID;
            delete(curFrame);
            char echo[4];
            memcpy(echo, echoID.b, 4);
            sendto(sock, echo, sizeof(echo), 0, (struct sockaddr *)&remoteAddr, addrlen);
            cout<<"echo reply sent!"<<endl;
            continue;
        }

        memcpy(tmp, &(buffer[8]), 4);
        curFrame->bufferSize = *(int*)tmp;
        cout<<"frame "<<curFrame->frmID<<" received, filesize: "<<curFrame->bufferSize;
        cout<<" at "<<setprecision(15)<<wallclock()<<endl;

        curFrame->buffer = new char[curFrame->bufferSize];
        memset(curFrame->buffer, 0, curFrame->bufferSize);

        memcpy(curFrame->buffer, &(buffer[12]), curFrame->bufferSize);
        
        pthread_mutex_lock(&fbhMutex);
        if(frameBufferHead == NULL) {
            frameBufferHead = curFrame;
            frameBufferTail = curFrame;
        }
        else {
            frameBufferTail->nextFrameBuffer = curFrame;
            frameBufferTail = curFrame;
        }
        pthread_mutex_unlock(&fbhMutex);
        //cout<<"frame "<<frmseq - 1<<" written into buffer"<<endl;
    }
    cout<<"Receiver Thread End!"<<endl;
}

void *ThreadSenderFunction(void *socket) {
    cout << "Sender Thread Created!" << endl;
    char buffer[RES_SIZE];

    struct resBuffer* curRes;

    int sock = *((int*)socket);

    pthread_mutex_lock(&rbhMutex);
    while (resBufferHead != NULL) {
        if (resBufferHead->markerNum.i != 0)
            delete(resBufferHead->buffer);
        void* nextResBuffer = resBufferHead->nextResBuffer;
        delete(resBufferHead);
        resBufferHead = (struct resBuffer*)nextResBuffer;
    }
    pthread_mutex_unlock(&rbhMutex);

    while (1) {
        if(!client_alive) break;
        pthread_mutex_lock(&rbhMutex);
        if(resBufferHead != NULL) {
            curRes = resBufferHead;
            pthread_mutex_unlock(&rbhMutex);

            memset(buffer, 0, sizeof(buffer));
            memcpy(buffer, curRes->resID.b, 4);
            memcpy(&(buffer[4]), curRes->resType.b, 4);
            memcpy(&(buffer[8]), curRes->markerNum.b, 4);
            if(curRes->markerNum.i != 0)
                memcpy(&(buffer[12]), curRes->buffer, 100 * curRes->markerNum.i);
            sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&remoteAddr, addrlen);

            cout<<"frame "<<curRes->resID.i<<" res sent, "<<"marker#: "<<curRes->markerNum.i;
            cout<<" at "<<setprecision(15)<<wallclock()<<endl<<endl;

            pthread_mutex_lock(&rbhMutex);
            if(curRes->markerNum.i != 0)
                delete(resBufferHead->buffer);
            void* nextResBuffer = resBufferHead->nextResBuffer;
            delete(resBufferHead);
            resBufferHead = (struct resBuffer*)nextResBuffer;
            pthread_mutex_unlock(&rbhMutex);
        }
        else {
            pthread_mutex_unlock(&rbhMutex);
            this_thread::sleep_for(chrono::milliseconds(5));
            continue;
        }
    }    
    cout<<"Sender Thread End!"<<endl;
}

void *ThreadProcessFunction(void *param) {
    cout<<"Process Thread Created!"<<endl;
    int n;
    int frmCount = 0;
    int frmID;
    int frmDataType;
    char* frmdata;
    int frmSize;
    struct resBuffer* curRes;

    const int MAX_COUNT = 20;
    bool markerDetected = false;
    bool returnRes = true;
    
    vector<Point2f> points[2];
    recognizedMarker marker;

    while (1) {
        if(!client_alive) break;
        pthread_mutex_lock(&fbhMutex);
        if(frameBufferHead == NULL) {
            pthread_mutex_unlock(&fbhMutex);
            this_thread::sleep_for(chrono::milliseconds(5));
            continue;
        }
        pthread_mutex_unlock(&fbhMutex);

        pthread_mutex_lock(&fbhMutex);
        frmID = frameBufferHead->frmID;
        frmDataType = frameBufferHead->dataType;
        frmSize = frameBufferHead->bufferSize;
        frmdata = frameBufferHead->buffer;
        pthread_mutex_unlock(&fbhMutex);
        frmCount++;
        
        if(frmDataType == IMAGE_DETECT) {
            vector<uchar> imgdata(frmdata, frmdata + frmSize);
            Mat img_scene = imdecode(imgdata, CV_LOAD_IMAGE_GRAYSCALE);
            markerDetected = query(img_scene, marker);
        }

        pthread_mutex_lock(&fbhMutex);
        delete(frameBufferHead->buffer);
        void* nextFrameBuffer = frameBufferHead->nextFrameBuffer;
        delete(frameBufferHead);
        frameBufferHead = (struct frameBuffer*)nextFrameBuffer;
        pthread_mutex_unlock(&fbhMutex);

        //transfer result back to phone
        curRes = new resBuffer;
        curRes->nextResBuffer = NULL;

        if(markerDetected) {
            charfloat p;
            curRes->resID.i = frmID;
            curRes->resType.i = BOUNDARY;
            curRes->markerNum.i = 1;
            curRes->buffer = new char[100 * curRes->markerNum.i];

            int pointer = 0;
            memcpy(&(curRes->buffer[pointer]), marker.markerID.b, 4);
            pointer += 4;
            memcpy(&(curRes->buffer[pointer]), marker.height.b, 4);
            pointer += 4;
            memcpy(&(curRes->buffer[pointer]), marker.width.b, 4);
            pointer += 4;

            for(int j = 0; j < 4; j++) {
                p.f = marker.corners[j].x;
                memcpy(&(curRes->buffer[pointer]), p.b, 4);
                pointer+=4;
                p.f = marker.corners[j].y;
                memcpy(&(curRes->buffer[pointer]), p.b, 4);        
                pointer+=4;            
            }

            memcpy(&(curRes->buffer[pointer]), marker.markername.data(), marker.markername.length());
        }
        else {
            curRes->resID.i = frmID;
            curRes->markerNum.i = 0;
        }

        pthread_mutex_lock(&rbhMutex);
        if(resBufferHead != NULL) {
            resBufferTail->nextResBuffer = curRes;
            resBufferTail = curRes;
        }
        else {
            resBufferHead = curRes;
            resBufferTail = curRes;
        }
        pthread_mutex_unlock(&rbhMutex);
    }

    cout<<"Process Thread End!"<<endl;
}

void runServer() {
    pthread_t senderThread, receiverThread, processThread;
    int ret1, ret2, ret3;
    char buffer[PACKET_SIZE];
    char fileid[4];
    int status = 0;
    int sock;
    
    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cout << "cannot create socket" << endl;
                exit(1);
    }
    memset((char*)&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(PORT);
    if(bind(sock, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
        cout << "bind failed" << endl;
                exit(1);
    }

    pthread_mutex_init(&fbhMutex, NULL);
    pthread_mutex_init(&rbhMutex, NULL);
    cout << endl << "========server started, waiting for clients==========" << endl;

    client_alive = true;
    ret1 = pthread_create(&receiverThread, NULL, ThreadReceiverFunction, (void *)&sock);
    ret2 = pthread_create(&processThread, NULL, ThreadProcessFunction, NULL);
    ret3 = pthread_create(&senderThread, NULL, ThreadSenderFunction, (void *)&sock);

    pthread_join(receiverThread, NULL);
    pthread_join(processThread, NULL);
    pthread_join(senderThread, NULL);

    pthread_mutex_destroy(&fbhMutex);
    pthread_mutex_destroy(&rbhMutex);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " size[s/m/l] NN#[1/2/3/4/5]" << endl;
        return 1;
    }

    parseCMD(argv);
    loadImages();
#ifdef TRAIN
    trainParams();
#else
    loadParams();
#endif
    encodeDatabase(); 
    //test();
    runServer();

    freeParams();
    return 0;
}
