/*
 * jsonio.cpp -- read and write json files
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
#include "jsonio.hpp"
//#include <iostream>

json::value jsonRead(std::string const& file, std::string const* path)
{
    //WEK move home get and file path logic to helper std::string getFileName(path, file)
    const char* homedir = getenv("HOME");
    if (homedir == nullptr) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != nullptr) {
            homedir = pw->pw_dir;
        }
    }
    std::string home(homedir);
    std::string jsonfile;
    if (path && std::filesystem::exists(*path + file)) {
        jsonfile = *path + file;
    } else if (std::filesystem::exists(home + "/racecam/data/" + file)) { 
		jsonfile = home + "/racecam/data/" + file;
	} else if (std::filesystem::exists("/usr/local/etc/racecam/data/" + file)) {
        jsonfile = "/usr/local/etc/racecam/data/" + file;
    } else {
        return nullptr;
    }

   std::ifstream ifile(jsonfile);
   if (!ifile.is_open()) return nullptr;
    json::stream_parser p;
    boost::system::error_code ec;
    do
    {
        char buf[4096]; 
        //WEK change to read a line at a time so error will be easier to find
		ifile.read(buf, sizeof(buf));
		p.write( buf, ifile.gcount(), ec );
    	if( ec ) {
            std::string istr(buf, ifile.gcount());
            std::cerr << "input buffer: " << istr << std::endl;
            throw std::runtime_error("JSON write error: " + ec.message() + " " + ec.to_string());
        }
    }
    while(!ifile.eof());
    p.finish( ec );
    if( ec ) throw std::runtime_error("JSON finish error: " + ec.message() + " " + ec.to_string());
    return p.release();
} 

void jsonWrite(std::string const& file, json::value const& jv, std::string const* path)
{
    // WEK use getFileName()
    const char* homedir = getenv("HOME");
    if (homedir == nullptr) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != nullptr) {
            homedir = pw->pw_dir;
        }
    }
    std::string home(homedir);
    std::string jsonfile;
    if (path && std::filesystem::exists(*path)) {
        jsonfile = *path + file;
    } else if (std::filesystem::exists(home + "/racecam/data/")) { 
		jsonfile = home + "/racecam/data/" + file;
	} else {
        std::filesystem::create_directories(home + "/racecam/data/");
        if (std::filesystem::exists("/usr/local/etc/racecam/data/")) {
            jsonfile = "/usr/local/etc/racecam/data/" + file;
        } else {
            throw std::runtime_error("jsonWrite() Unable to find path for output! file: " + file);
        }
    }

    std::ofstream ofile (jsonfile, std::ios::out | std::ios::trunc);
    if (ofile.is_open())
    {
	jsonWrite(ofile, jv);
	ofile.close();
    } else
	throw std::runtime_error("Unable to open jsonfile for output! file: " + jsonfile);
}

void jsonWrite(std::ostream& os, json::value const& jv, std::string* indent)
{
    std::string indent_;
    if(! indent)
        indent = &indent_;
    switch(jv.kind())
    {
    case json::kind::object:
    {
        os << "{\n"; 
        indent->append(4, ' ');
        auto const& obj = jv.get_object();
        if(! obj.empty())
        {
            auto it = obj.begin();
            for(;;)
            {
                os << *indent << json::serialize(it->key()) << " : ";
                jsonWrite(os, it->value(), indent);
                if(++it == obj.end())
                    break;
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "}";
        break;
    }

    case json::kind::array:
    {
        std::string space = " ";
        os << "[";
        indent->append(4, ' ');
        auto const& arr = jv.get_array();
        if(! arr.empty())
        {
            auto it = arr.begin();
            for(;;)
            {
                if (it->is_object()) {
                    os << "\n" << *indent;
                    jsonWrite( os, *it, indent);
                } else {
                    jsonWrite( os, *it, &space);
                }
                if(++it == arr.end())
                    break;
                os << ", ";
            }
        }
        indent->resize(indent->size() - 4);
        os << "]";
        break;
    }

    case json::kind::string:
    {
        os << json::serialize(jv.get_string());
        break;
    }

    case json::kind::uint64:
    case json::kind::int64:
    case json::kind::double_:
        os << jv;
        break;

    case json::kind::bool_:
        if(jv.get_bool())
            os << "true";
        else
            os << "false";
        break;

    case json::kind::null:
        os << "null";
        break;
    }

    if(indent->empty())
        os << "\n";
}
