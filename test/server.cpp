#include <iostream>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <iomanip>
#include <queue>
#include <fstream>

#define PACKET_SIZE 50000
#define RES_SIZE 512

using namespace std;

struct sockaddr_in localAddr;
struct sockaddr_in remoteAddr[5];
socklen_t addrlen = sizeof(localAddr);

void *ThreadReceiverFunction(void *socket) {
    cout<<"Receiver Thread Created!"<<endl;
    char buffer[RES_SIZE];
    int sock = *((int*)socket);

    while (1) {
        for(int i = 0; i < 5; i++) {
            memset(buffer, 0, sizeof(buffer));
            recvfrom(sock, buffer, PACKET_SIZE, 0, (struct sockaddr *)&remoteAddr[0], &addrlen);
            int markerNum = *(int*)&(buffer[8]);
            cout<<"result received with "<<markerNum<<" markers"<<endl;
        }
    }
}

void *ThreadSenderFunction(void *socket) {
    cout << "Sender Thread Created!" << endl;
    char buffer[PACKET_SIZE];
    int sock = *((int*)socket);

    ifstream in("request", ios::in | ios::binary);
    in.read(buffer, 48713);
    in.close();

    for(int test = 0; test < 60 * 8; test++) {
        this_thread::sleep_for(chrono::milliseconds(1000));

        for(int i = 0; i < 5; i++) {
            sendto(sock, buffer, 48713, 0, (struct sockaddr *)&remoteAddr[i], addrlen);
            cout<<"request sent to container "<<i<<endl;
        }
    }    
}

int main(int argc, char *argv[]) {
    pthread_t senderThread, receiverThread;
    int sockUDP;

    memset((char*)&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(51710);

    struct hostent *hp;
    hp = gethostbyname("192.168.50.223");
    for(int i = 0; i < 5; i++) {
        memset((char*)&remoteAddr[i], 0, sizeof(remoteAddr[i]));
        remoteAddr[i].sin_family = AF_INET;
        remoteAddr[i].sin_port = htons(51717+i);
        memcpy((void*)&remoteAddr[i].sin_addr, hp->h_addr_list[0], hp->h_length);
    }

    if((sockUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cout<<"ERROR opening udp socket"<<endl;
        exit(1);
    }
    if(bind(sockUDP, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
        cout<<"ERROR on udp binding"<<endl;
        exit(1);
    }

    pthread_create(&receiverThread, NULL, ThreadReceiverFunction, (void *)&sockUDP);
    pthread_create(&senderThread, NULL, ThreadSenderFunction, (void *)&sockUDP);

    pthread_join(receiverThread, NULL);
    pthread_join(senderThread, NULL);
}
