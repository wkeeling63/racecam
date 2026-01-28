/* youtube.hpp - youtube class.
 */
 
#pragma once

//#include <iostream>
//#include <fstream>
#include <thread>
//#include <chrono>
//#include <filesystem>

#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>

//#include <boost/json/src.hpp> 
//#include <boost/json.hpp>  //WEK can't figure out how to make meson link to boost_json 1.87 -- try on new build 

//#include <ctime>
#include <sys/stat.h>

namespace json = boost::json;

typedef struct {
	json::object resp;
	int respcode;
} rsp_type;

typedef struct {
	std::string b_id;
	std::string s_id;
	std::string strmurl;
} yt_strm;

//WEK does this need to be in client.json?
const std::string apikey = "AIzaSyBOjJlttYVt-90r9Zo2eHY7CCnSQrqKDpI";

// enum publish {pub, unl, priv };

class YouTube
{
public:
	YouTube(std::string const) ;
	~YouTube();

json::value GetAuth(bool = true);
//WEK pass FPS and Resolution as numbers and convert here or pass ass string and convert in caller?
/*
240p = 320x240
360p = 640x360
480p = 640x480
720p = 1280x720
1080p = 1920x1080
1440p = 2560x1440
2160p = 3840x2160
variable = both rate and resolution must be variable
* 
* 
30fps
60fps
variable = both rate and resolution must be variable
*
* I guess use variable for now so nothing needs to be passed or decoded
*/
yt_strm StartStrm(std::string const&, std::string const&);
void StopStrm(std::string const&);

private:

size_t getRespJson(char*, size_t, size_t);
rsp_type postMsg(std::string const&, std::string const&);
rsp_type postMsg(std::string const&, std::string const&, std::list<std::string> const&);
// std::string insBroadcast(std::string const&, std::string const&);
	
	std::string path_;
	json::stream_parser rp_;
	std::string id_ {};
	std::string secret_ {};
};
