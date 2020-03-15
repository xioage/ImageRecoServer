#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

using namespace std;

#define MAXPROCESS 10

string server = "./gpu_fv s l 5 51717 ";
string queryNum;
string output = " > scalabilityTest/";
string fileName;
string suffix = ".txt &";

string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

string composeCmd() {
    string cmd = server + queryNum + output + fileName + suffix;
    cout<<cmd<<endl;
    return cmd;
}

int main() {
    string cmd;

    for(int u = 1; u < 100; u+= 4) {
	if(u < MAXPROCESS) {
	    for(int i = 0; i < u; i++) {
	        queryNum = to_string(1);
	        fileName = to_string(u * 100 + i);
		cmd = composeCmd();
                //exec(cmd.c_str());
	    } 
	}
	else {
	    int baseNum = u / MAXPROCESS;
	    int extraNum = u % MAXPROCESS;
	    for(int i = 0; i < MAXPROCESS; i++) {
		if(i < extraNum) queryNum = to_string(baseNum+1);
		else             queryNum = to_string(baseNum);
	        fileName = to_string(u * 100 + i);
		cmd = composeCmd();
                //exec(cmd.c_str());
	    }
	}
    }
    return 0;
}
