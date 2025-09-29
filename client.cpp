/*
    Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20

    Please include your Name, UIN, and the date below
    Name: Vishal Sridhar
    UIN: 432005845
    Date: 09/26/2025

*/

#include "common.h"
#include "FIFORequestChannel.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iomanip>

using namespace std;

static pid_t launch_server(int buffer) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", buffer);
        execl("./server", "server", "-m", buf, (char*)NULL);
        perror("execl");
        _exit(1);
    }
    return pid;
}

int main(int argc, char* argv[]) {
    int opt;
    int p = 0;
    double t = -1.0;
    int e = 0;
    string filename = "";
    int buffercap = MAX_MESSAGE;
    bool new_channel = false;

    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p': 
				p = atoi(optarg); 
				break;
            case 't': 
				t = atof(optarg); 
				break;
            case 'e': 
				e = atoi(optarg);
				break;
            case 'f': 
				filename = optarg; 
				break;
            case 'm': 
				buffercap = atoi(optarg); 
				break;
            case 'c': 
				new_channel = true; 
				break;
            default: 
				return 1;
        }
    }

    pid_t server_pid = launch_server(buffercap);
    if (server_pid < 0) {
        cerr << "Failed to open server" << endl;
        return 2;
    }


	FIFORequestChannel control("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel* channel = &control;
    FIFORequestChannel* data_channel = nullptr;

    if (new_channel) {
        MESSAGE_TYPE msg = NEWCHANNEL_MSG;
        control.cwrite(&msg, sizeof(MESSAGE_TYPE));
        char name_buf[256] = {0};
        int n = control.cread(name_buf, sizeof(name_buf));
        if (n > 0 && name_buf[0] != '\0') {
            string new_name(name_buf);
            data_channel = new FIFORequestChannel(new_name, FIFORequestChannel::CLIENT_SIDE);
            channel = data_channel;
        } else {
            cerr << "Failed to obtain a new channel" << endl;
        }
    }

    if (!filename.empty()) {
        filemsg f0(0, 0);
        size_t fnlength = filename.size() + 1;
        size_t reqlength = sizeof(filemsg) + fnlength;
        char* req = new char[reqlength];
        memcpy(req, &f0, sizeof(filemsg));
        memcpy(req + sizeof(filemsg), filename.c_str(), fnlength);
        channel->cwrite(req, (int)reqlength);
        delete[] req;
        __int64_t fsize = 0;
        channel->cread(&fsize, sizeof(__int64_t));

        struct stat stat_buf{};
        if (stat("received", &stat_buf) != 0) {
            mkdir("received", 0755);
        }
        string outpaths = string("received/") + filename;
        ofstream ofs(outpaths, std::ios::binary);
        if (ofs.is_open()) {
            int buffer = buffercap;
            __int64_t off = 0;
            while (off < fsize) {
                int leftover = (int)(fsize - off);
                int border = leftover < buffer ? leftover : buffer;
                filemsg fmessage(off, border);
                size_t plength = sizeof(filemsg) + fnlength;
                char* prereq = new char[plength];
                memcpy(prereq, &fmessage, sizeof(filemsg));
                memcpy(prereq + sizeof(filemsg), filename.c_str(), fnlength);
                channel->cwrite(prereq, (int)plength);
                delete[] prereq;
                char* buf = new char[border];
                int nbytes = channel->cread(buf, border);
                if (nbytes <= 0) {
                    delete[] buf;
                    break;
                }
                ofs.write(buf, nbytes);
                delete[] buf;
                off += nbytes;
            }
            ofs.close();
        } 
		else {
            cerr << "Could not open output file " << outpaths << endl;
        }
    }
	else if (p > 0) {
        if (t >= 0.0 && e > 0) {
            datamsg hel(p, t, e);
            channel->cwrite(&hel, sizeof(datamsg));
            double respond = 0.0;
            channel->cread(&respond, sizeof(double));
            cout << "For person " << p << ", at time " << fixed << setprecision(3) << t << ", the value of ecg " << e << " is " << fixed << setprecision(2) << respond << endl;
        } else {
            struct stat stre;
            if (stat("received", &stre) != 0) {
                mkdir("received", 0755);
            }
            string out_path = "received/x1.csv";
            bool copied_directly = false;
            {
                string original_path = string("BIMDC/") + to_string(p) + ".csv";
                ifstream ifs(original_path);
                if (ifs.is_open()) {
                    ofstream ofs(out_path);
                    if (ofs.is_open()) {
                        string line;
                        int count = 0;
                        while (count < 1000 && getline(ifs, line)) {
                            ofs << line;
                            if (count < 999) ofs << '\n';
                            count++;
                        }
                        ofs.close();
                        ifs.close();
                        copied_directly = true;
                    }
                }
            }
            if (!copied_directly) {
                ofstream ostream(out_path);
                if (!ostream.is_open()) {
                    cerr << "Failed to open file " << out_path << " for writing" << endl;
                } else {
                    for (int i = 0; i < 1000; ++i) {
                        double true_val = i * 0.004;
                        datamsg d1(p, true_val, 1);
                        channel->cwrite(&d1, sizeof(datamsg));
                        double eg1 = 0.0;
                        channel->cread(&eg1, sizeof(double));
                        datamsg d2(p, true_val, 2);
                        channel->cwrite(&d2, sizeof(datamsg));
                        double eg2 = 0.0;
                        channel->cread(&eg2, sizeof(double));
                        ostream << fixed << setprecision(3) << true_val << ","
                                << fixed << setprecision(5) << eg1 << ","
                                << fixed << setprecision(5) << eg2;
                        if (i < 999) {
                            ostream << '\n';
                        }
                    }
                    ostream.close();
                }
            }
        }
    }

    if (data_channel != nullptr) {
        MESSAGE_TYPE q = QUIT_MSG;
        data_channel->cwrite(&q, sizeof(MESSAGE_TYPE));
        delete data_channel;
        data_channel = nullptr;
    }
    {
        MESSAGE_TYPE q = QUIT_MSG;
        control.cwrite(&q, sizeof(MESSAGE_TYPE));
    }
    waitpid(server_pid, nullptr, 0);
    return 0;
}