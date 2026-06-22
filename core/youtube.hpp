/* youtube.hpp - youtube class.
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
 
#pragma once

#include <thread>

#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>

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
//const std::string apikey = "AIzaSyBOjJlttYVt-90r9Zo2eHY7CCnSQrqKDpI";

// enum publish {pub, unl, priv };

class YouTube
{
public:
	YouTube() ;
	~YouTube();

json::value GetAuth(bool = true);
//WEK pass FPS and Resolution as numbers and convert here or pass as string and convert in caller?
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
	
	json::stream_parser rp_;
	std::string id_ {};
	std::string secret_ {};
};
