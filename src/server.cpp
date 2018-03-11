#include "reco.hpp"
#include "cuda_files.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <chrono>
#include <thread>

#define NUM_CLUSTERS 32 
#define MESSAGE_ECHO 0
#define FEATURES 1
#define IMAGE_DETECT 2
#define BOUNDARY 3
#define PORT 51717
#define PACKET_SIZE 60000
#define RES_SIZE 512

using namespace std;
using namespace cv;
using namespace std::chrono;

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

std::vector<struct recognizedMarker> recognizedMarkers;

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
		cout<<"frame "<<curFrame->frmID<<" received, filesize: "<<curFrame->bufferSize<<endl;

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

			cout<<"frame "<<curRes->resID.i<<" res sent, "<<"marker#: "<<curRes->markerNum.i<<endl;

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
	chrono::time_point<chrono::system_clock> st_o, st_n, st_s;
	int time_diff;
	int frmCount = 0;
	double curr_fps, avg_fps;
	int frmID;
	int frmDataType;
	char* frmdata;
	int frmSize;
	struct resBuffer* curRes;
	string wndname;

	const int MAX_COUNT = 20;
	bool markerDetected = false;
	bool returnRes = true;
	bool initial = true;
	bool showfps = false;
	char bitmap[2][MAX_COUNT];
	
	Mat gray, prevGray;
	vector<Point2f> points[2];

	st_o = chrono::system_clock::now();
	st_s = st_o;

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
			//img_scene = imdecode(imgdata, CV_LOAD_IMAGE_GRAYSCALE);

			markerDetected = false;

			if (recognizedMarkers.size() > 0)
				markerDetected = true;
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
			curRes->markerNum.i = recognizedMarkers.size();
			curRes->buffer = new char[100 * curRes->markerNum.i];

			int pointer;
			recognizedMarker marker;
			for(int i = 0; i < curRes->markerNum.i; i++) {
				pointer = 100 * i;
				marker = recognizedMarkers[i];

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

	recognizedMarkers.clear();
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
	cout << "========server started, waiting for clients==========" << endl;

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

#if 0
int posterClassification(int index) {
	int m;
	//Rect area = poster_rects.at(i);
    Mat testDescriptor, descriptors_scene;
	//Mat poster = Mat(img_scene, area).clone();
	Mat poster = img_scene;

	if (false) {
		string wndname = "poster" + to_string(index);
		imshow(wndname, poster);
		waitKey(1);
	}
	
	//high_resolution_clock::time_point t0 = high_resolution_clock::now();
	vector<KeyPoint> cur_keypoints;

    string min_class_label = "demo_1";
	string posterName = min_class_label.substr(0, min_class_label.size() - 2);
	cout <<"cur poster: "<<posterName<<endl;
	//if (min_pred < -2) return;

	cur_keypoints.clear();
	detectorM->detect(poster, cur_keypoints);
	surf_extractor->compute(poster, cur_keypoints, descriptors_scene);
	vector<struct posterImg> posters = poster_imgs_all[min_class_label];
	//cout<<"posters.size: "<<posters.size()<<endl;

	bool matchFound = false;
	for (m = index; m < posters.size(); m++) {
		struct posterImg *poster = &(posters[m]);
		Mat descriptors_marker = poster->descriptors;
		vector<KeyPoint> keypoints_marker = poster->keypoints;

		vector< DMatch > matches, good_matches, good_matches_nxt;
		vector<Point2f> obj, scene;
		matcherT->match(descriptors_marker, descriptors_scene, matches);
		//high_resolution_clock::time_point t2 = high_resolution_clock::now();
		//cout << "marker key points: " << keypoints_marker.size() << endl;
		//cout << "scene key points: " << cur_keypoints.size() << endl;
		//cout << "matching time: " << duration_cast<microseconds>(t2 - t1).count() << endl;

		double max_dist = 0;
		double min_dist = 100;

		//-- Quick calculation of max and min distances between keypoints
		for (int i = 0; i < descriptors_marker.rows; i++) {
			double dist = matches[i].distance;
			if (dist < min_dist) min_dist = dist;
			if (dist > max_dist) max_dist = dist;
		}
		//-- Pick only "good" matches (i.e. whose distance is less than 3*min_dist )
		for (int i = 0; i < descriptors_marker.rows; i++) {
			if (matches[i].distance < 3 * min_dist) {
				good_matches.push_back(matches[i]);
			}
		}
		
		if (good_matches.size() < 10) continue;

		for (int i = 0; i < good_matches.size(); i++) { //-- Get the keypoints from the good matches			
			obj.push_back(keypoints_marker[good_matches[i].queryIdx].pt);
			scene.push_back(cur_keypoints[good_matches[i].trainIdx].pt);
		}

		Mat H = findHomography(obj, scene, CV_RANSAC);

		vector<Point2f> obj_corners(4), scene_corners(4);
		obj_corners[0] = cvPoint(0, 0); 
		obj_corners[1] = cvPoint(poster->cols, 0);
		obj_corners[2] = cvPoint(poster->cols, poster->rows); 
		obj_corners[3] = cvPoint(0, poster->rows);

		try {
			perspectiveTransform(obj_corners, scene_corners, H);
		}
		catch (Exception) {
			cout << "cv exception" << endl;
			continue;
		}

		if (contourArea(scene_corners) < 5000 || !isContourConvex(scene_corners)) continue;

		recognizedMarker marker;
		marker.markerID.i = stoi(poster->imgID) - 1;
		cout << "ID: " << poster->imgID << endl;
		marker.height.i = poster->rows;
		marker.width.i = poster->cols;
		for (int i = 0; i < 4; i++) {
			//marker.corners[i].x = scene_corners[i].x + area.tl().x;
			//marker.corners[i].y = scene_corners[i].y + area.tl().y;
			marker.corners[i].x = scene_corners[i].x;
			marker.corners[i].y = scene_corners[i].y;
		}
		marker.markername = posterName+".";
		recognizedMarkers.push_back(marker);

		matches.clear();
		matchFound = true;
		break;
	}
	matchFound ? cout << "match found: " << m << endl : cout << "no match found!" << endl;
	//high_resolution_clock::time_point t3 = high_resolution_clock::now();
	//cout << "matching time full: " << duration_cast<microseconds>(t3 - t1).count() << endl;
	return m;
}
#endif

int main(int argc, char *argv[])
{
    int querysizefactor, nn_num;

    if (argc < 3) {
        cout << "Usage: " << argv[0] << " size[s/m/l] NN#[1/2/3/4/5]" << endl;
        return 1;
    }
    if (argv[1][0] == 's') querysizefactor = 4;
    else if (argv[1][0] == 'm') querysizefactor = 2;
    else querysizefactor = 1;
    nn_num = argv[2][0] - '0';
    if (nn_num < 1 || nn_num > 5) nn_num = 5;

    float *means, *covariances, *priors, *projectionCenter, *projection;
    int dimension = 82;

    gpu_init();

    vector<char *> whole_list = loadImages();

   //if(false) trainParams(whole_list, dimension, means, covariances, priors, projectionCenter, projection);
    loadParams(dimension, means, covariances, priors, projectionCenter, projection);

    gpu_copy(covariances, priors, means, NUM_CLUSTERS, dimension);

    vector<float> train[whole_list.size()];
    //Encode train files
    for (int i = 0; i < whole_list.size(); i++) {
        SiftData tmp;
        onlineProcessing(whole_list[i], tmp, priors, means, covariances, train[i], projection, projectionCenter, false);
    }

    test(priors, means, covariances, projection, projectionCenter, train, whole_list.size(), nn_num);

    gpu_free();
    free(projection);
    free(projectionCenter);
    free(priors);
    free(means);
    free(covariances);

    return 0;
}
