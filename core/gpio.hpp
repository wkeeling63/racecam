/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * gpio.hpp - LED and Switch RaceCam class.
 */

#pragma once
#include <fcntl.h> 
#include <unistd.h>
#include <termios.h> 

#define HEX( x )   std::setw(2) << std::setfill('0') << std::hex << (int)( x )
// WEK move i/ostreams to iostream and move opens/closes to constructor/distructor
// read/seek/write to toggle
class GPIO {
public:
    
GPIO() {
    flags_ = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags_ == -1) {
        // Handle error
	throw std::runtime_error("STDIN fcntl getfl failed");
    }
    if (fcntl(STDIN_FILENO, F_SETFL, flags_ | O_NONBLOCK) == -1) {
        // Handle error
	throw std::runtime_error("STDIN fcntl setfl failed");
    }
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, &old_tio_); // Save old settings
    new_tio = old_tio_;
    new_tio.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio); // Apply new settings
}

~GPIO() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio_);
}

char get() {
//	fprintf(stdout, "%s:%s:%d \n", __FILE__, __PRETTY_FUNCTION__, __LINE__);
    char c;
    ssize_t bytes_read = read(STDIN_FILENO, &c, 1);
    if (bytes_read == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available, non-blocking read returned immediately
	    return 0;
	} else {
            // Handle other read errors
	    throw std::runtime_error("STDIN read " + std::string(std::strerror(errno)));
	} 
    } else if (bytes_read == 0) {
        // End of file (stdin closed)
	std::cout << "end of file return" << std::endl;
	return 0;
    } else {
	c -= 48;
	if (c == 1 || c == 2) return c;
	return 0;
    }
}
void set(int led, bool val) {
    std::string fpath = "/sys/class/leds/LED" + std::to_string(led) + "/brightness";
//    std::ofstream f(fpath);
    std::ofstream f("/sys/class/leds/LED" + std::to_string(led) + "/brightness");
    if (!f.is_open()) throw std::runtime_error("Failed to open led: " + fpath);
    std::string bstr = val ? "1" : "0";
    f << bstr;
    if (f.fail()) throw std::runtime_error("Failed to write led: " + fpath);
    f.close();
}

void toggle(int led) {
    std::string fpath = "/sys/class/leds/LED" + std::to_string(led) + "/brightness";
//	std::cout << fpath << std::endl;
    std::ifstream in(fpath);
    if (!in.is_open()) throw std::runtime_error("Failed to open in led: " + fpath);
    std::string bstr((std::istreambuf_iterator<char>(in)),
    std::istreambuf_iterator<char>());
    in.close();

    std::ofstream out(fpath);
    if (!out.is_open()) throw std::runtime_error("Failed to open out led: " + fpath);
    bool b = !(std::stoi(bstr));
    bstr = b ? "1" : "0";
    out << bstr;
    if (out.fail()) throw std::runtime_error("Failed to write led: " + fpath);
    out.close();
}

private:
    int flags_;
    struct termios old_tio_;
	
};
