#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>

using namespace std;

#define MAXPROCESS 10

string server = "./gpu_fv s l 5 51717 ";
string queryNum;
string output = " > scalabilityTest/";
string fileName;
string suffix = ".txt &";
string tail = "tail -n 1 scalabilityTest/";
string attach = ".txt >> scalabilityTest/";
string suffix2 = ".txt";

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
    //cout<<cmd<<endl;
    return cmd;
}

string composeCmd2(int u) {
    string cmd = tail + fileName + attach + to_string(u) + suffix2;
    //cout<<cmd<<endl;
    return cmd;
}

double processResult(int u) {
    string line;
    string fileName = "scalabilityTest/" + to_string(u) + ".txt";
    ifstream file(fileName);

    int fileLength = u > MAXPROCESS ? MAXPROCESS : u;
    double total = 0;
    for(int row = 0; row != fileLength; ++row) {
        getline(file, line);
	total += stod(line);
    }
    file.close();

    double average = total/u;
    std::cout << "" << u << ": " << average << std::endl;

    return average;
}

int main() {
    string cmd;

    exec("rm scalabilityTest/*.txt");

    for(int u = 1; u < 20; u+= 4) {
	if(u < MAXPROCESS) {
	    for(int i = 0; i < u; i++) {
	        queryNum = to_string(1);
	        fileName = to_string(u * 100 + i);
		cmd = composeCmd();
                exec(cmd.c_str());
	    } 
	    
	    this_thread::sleep_for(chrono::seconds(30));
            
	    for(int i = 0; i < u; i++) {
	        fileName = to_string(u * 100 + i);
		cmd = composeCmd2(u);
		exec(cmd.c_str());
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
                exec(cmd.c_str());
	    }

	    this_thread::sleep_for(chrono::seconds(30));

	    for(int i = 0; i < MAXPROCESS; i++) {
	        fileName = to_string(u * 100 + i);
		cmd = composeCmd2(u);
		exec(cmd.c_str());
	    }
	}

	processResult(u);
    }
    return 0;
}
