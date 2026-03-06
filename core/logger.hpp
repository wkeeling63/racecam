/*
 * logger.hpp logging class
 * 
 * RaceCam Is an app for multiple camera video capture both locally and streaming.
 * Copyright (C) <2026> <William Keeling>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
 
#ifndef RACECAM_LOGGER_H
#define RACECAM_LOGGER_H

#include <fstream>
#include <filesystem>

extern "C" {
#include "libavutil/log.h"
}

/* libcamera::LoggingTargetNone, libcamera::LoggingTargetSyslog, libcamera::LoggingTargetFile, libcamera::LoggingTargetStream 
 	logSetTarget(LoggingTargetNone);   //libcamera logging
    WEK FYI you can't call logSetTarget twice -- so it can't be here or we need some sort of 
     DEBUG, INFO, WARN, ERROR and FATAL
	logSetLevel("*", "INFO"); //("Camera", "INFO");

 ffmpeg levels
	AV_LOG_QUIET   -8 	Print no output.
   	AV_LOG_PANIC   0  	Something went really wrong and we will crash now.
   	AV_LOG_FATAL   8  	Something went wrong and recovery is not possible.
   	AV_LOG_ERROR   16  	Something went wrong and cannot losslessly be recovered.
   	AV_LOG_WARNING   24 Something somehow does not look correct.
   	AV_LOG_INFO   32  	Standard information.
   	AV_LOG_VERBOSE   40 Detailed information.
    AV_LOG_DEBUG   48	Stuff which is only useful for libav* developers. */

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    ALWAYS
};

static class Logger* global_logger_ptr = nullptr;

using namespace libcamera;

class ThreadSafeOStream : public std::basic_streambuf<char> {
public:
    ThreadSafeOStream(std::ostream &os, std::mutex &mutex) : os_(os), mutex_(mutex) {}

protected:
    virtual std::streamsize xsputn(const char *s, std::streamsize n) {
        std::lock_guard<std::mutex> lock(mutex_); 
        os_.write(s, n);
        os_.flush();
        return n;
    }

    virtual int overflow(int c) {
        if (c != EOF) {
            std::lock_guard<std::mutex> lock(mutex_);
            os_.put(static_cast<char>(c));
            os_.flush();
        }
        return c;
    }
private:
    std::ostream &os_;
    std::mutex &mutex_;
};

namespace fs = std::filesystem;

class Logger {
public:
//WEK make filesystem calls use the noexecpt format
Logger(const std::string& filename, const LogLevel& level = LogLevel::ERROR) 
            : filename_(filename), level_(level) {
    std::string ofile, nfile;
    if (fs::exists(filename_)) {
        if (fs::file_size(filename_) >= 2097152) {
            for (int i = 8; i > -1; i--) {
                nfile = filename_ + "." + std::to_string(i + 1);
                ofile = (i ? filename_ + "." + std::to_string(i) : filename_); 
                if (fs::exists(ofile)) {
                    if ( i == 8 ) {
                        fs::remove(nfile);  
                    } else {
                        fs::rename(ofile,nfile);
                    }
                }
            }
        }
    }
    outputfile_.open(filename_, std::ios::app); 
    if (!outputfile_.is_open()) {
        std::cerr << "Error: Could not open log file: " << filename_ << std::endl;  //WEK make a throw
    }
    logSetStream(&outputfile_);
    setLevelAPIs(level_);
    outputfile_ << getTimestamp() << " [" << getLevelString(LogLevel::INFO) 
            <<  "] " << "Logging initialized at " << getLevelString(level_) << " level" << std::endl;
    if (global_logger_ptr != nullptr) {
            throw std::runtime_error("Only one instance of MyFFmpegLogger is allowed!");
        }
    av_log_set_callback(&Logger::FfmpegLogCallback);
    global_logger_ptr = this;
}

~Logger() {
        if (outputfile_.is_open()) {
            outputfile_.close();
        }
        av_log_set_callback(av_log_default_callback);
        global_logger_ptr = nullptr;
    }

void Log(LogLevel level, const std::string& message, bool tocout = false) {
    if (level >= level_) outputfile_ << getTimestamp() <<  " [" << 
        getLevelString(level) << "] " << message << std::endl;
    if (tocout) std::cout << message << std::endl;
}

void LogNR(LogLevel level, const std::string& message, bool tocout = false) {
    if (level >= level_) outputfile_ << getTimestamp() <<  " [" << 
        getLevelString(level) << "] " << message;
    if (tocout) std::cout << message;
}

void    SetLevel(LogLevel newlevel) {
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        level_ = newlevel;
        setLevelAPIs(level_);
    }
    outputfile_ << getTimestamp() << " [" << getLevelString(LogLevel::INFO) 
            <<  "] " << "Log level set to " << getLevelString(level_) << std::endl;
}

private:
    std::ofstream outputfile_;
    std::mutex log_mutex_;
    ThreadSafeOStream ts_out_ {outputfile_, log_mutex_}; 
    std::ostream shared_log_stream {&ts_out_};
    std::string filename_;
    LogLevel level_;
    
    static void FfmpegLogCallback(void* ptr, int fflevel, const char* fmt, va_list vargs) {
        if (!global_logger_ptr) throw std::runtime_error("nullptr for Logger!");
        if (ptr) {
            char buffer[4096];
            int print_prefix = true;
            av_log_format_line(ptr, fflevel, fmt, vargs, buffer, sizeof(buffer), &print_prefix);
            static const std::map<int, LogLevel> f2l = {
                { 16, LogLevel::ERROR },
                { 24, LogLevel::WARN },
                { 32, LogLevel::INFO },
                { 48, LogLevel::DEBUG }
            }; 
            std::string ffmpeg_message {"FFMPEG:"}; 
            ffmpeg_message.append(buffer);
      
            for (const auto& l : f2l) {
                if (fflevel <= l.first) {  // WEK add check to avoid call in messages not to be logged???
                    global_logger_ptr->LogNR(l.second, ffmpeg_message);
                    break;
                }
            }
        }
    }

    std::string getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    std::string getLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::ALWAYS: return "NOTICE";
            default:              return "UNKNOWN";
        }
    }
    
    void setLevelAPIs(LogLevel level){
        static const std::map<LogLevel, std::string> l2l = {
            { LogLevel::DEBUG, "DEBUG" },
            { LogLevel::INFO, "INFO" },
            { LogLevel::WARN, "WARN" },
            { LogLevel::ERROR, "FATAL" }
        }; 
        auto itl = l2l.find(level);
        if (itl != l2l.end()) {
            logSetLevel("*", itl->second.c_str());
        } else {
            logSetLevel("*", "ERROR");
        }
        static const std::map<LogLevel, int> l2f = {
            { LogLevel::DEBUG, 48 },
            { LogLevel::INFO, 32 },
            { LogLevel::WARN, 24 },
            { LogLevel::ERROR, 16 }
        }; 
        auto itf = l2f.find(level);
        if (itf != l2f.end()) {
            av_log_set_level(itf->second); 
        } else {
            av_log_set_level(AV_LOG_ERROR); 
        }
    }
};

#endif // RACECAM_LOGGER_H
