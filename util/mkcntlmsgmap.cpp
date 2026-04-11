/* 
 * mkcntlmsgmap.cpp
 * 
 * g++ -o mkcntlmap <path>/racecam/util/mkcntlmsgmap.cpp -I/usr/local/include -L/usr/local/lib -lyaml-cpp
 * 
 * utility programe to build map of input controls for racecam_config
 * from control_ids_*.yaml files.
 * 
 * download yaml file 
 * change to that dir
 * compile run to build cntl_msg_map.hpp
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
#include <iostream>
#include <string>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace fs = std::filesystem;

void reformat(std::string& s, bool tab) {
    size_t pos = 0;
    std::string repl {"\\\""};
    while ((pos = s.find("\"", pos)) != std::string::npos) {
        s.replace(pos, 1, repl);
        pos += repl.length();
    }
    
    // change \sa to See also:
    pos = 0;
    repl = "See also:";
    while ((pos = s.find("\\sa", pos)) != std::string::npos) {
        s.replace(pos, 3, repl);
        pos += repl.length();
    }
    
    // change \par to a tab
    pos = 0;
    repl = "\\t";
    while ((pos = s.find("\\par", pos)) != std::string::npos) {
        s.replace(pos, 4, repl);
        pos += repl.length();
    }
    // \f$LP = \frac{1\mathrm{m}}{D}\f$ to FP=1m/D
        pos = 0;
    repl = "FP = 1m/D";
    while ((pos = s.find("\\f$LP = \\frac{1\\mathrm{m}}{D}\\f$", pos)) != std::string::npos) {
        s.replace(pos, 32, repl);
        pos += repl.length();
    }
    
    pos = 0;
    repl = "\\n";
    if (tab) repl = "\\n\\t";
    while ((pos = s.find("\n", pos)) != std::string::npos) {
        if (pos == s.length() -1) {
            s.replace(pos, 1, "\\n");
        } else {
            s.replace(pos, 1, repl);
        }
        pos += repl.length();
    }
    // if last not \n add one
//    char last = *s.rbegin(); 
//    if ('\n' != last) s += "\\n";
    if ((pos = s.find("\\n", s.size()-2)) == std::string::npos) s += "\\n";
}

int main() {
     std::ofstream outfile("cntl_msg_map.hpp");
    if (!outfile.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return 1; 
    }
    outfile << "static const std::map<std::string, std::string> cntl_msg_map = {" << std::endl;
    fs::path current_dir = fs::current_path();
    for (const auto& entry : fs::directory_iterator(current_dir)) {
        if (entry.is_regular_file()) {
            if (entry.path().extension() == ".yaml") {
                std::cout << "processing file: " << entry.path().filename() << std::endl;
                try {
                    YAML::Node cntlyaml = YAML::LoadFile(entry.path().filename());
                    if (cntlyaml["controls"]) {
                        YAML::Node cntldesc = cntlyaml["controls"];
                        std::string message;
                        std::string desc;
                        for (const auto& cntls : cntldesc) {
                            for (const auto& key_value : cntls) {
                                if (key_value.second["direction"].as<std::string>() == "out") continue;
                                int keysize = key_value.first.as<std::string>().length();
                                outfile << "\t{\"" << key_value.first.as<std::string>() << "\", ";
                                message = key_value.first.as<std::string>() + "\\n\\n";
                                desc = key_value.second["description"].as<std::string>();
                                reformat(desc, false);
                                message += desc;
                                if (key_value.second["enum"]) {
                                    for (const auto& enums : key_value.second["enum"]) {
                                        std::string opts;
                                        for (const auto& key_value : enums) {
                                            if (key_value.first.as<std::string>() == "name")  continue;
                                            if (key_value.first.as<std::string>() == "value") 
                                                opts += key_value.second.as<std::string>() + "\\t";
                                            else {
                                                desc = key_value.second.as<std::string>();
                                                reformat(desc, true);
                                                opts += desc;
                                            }
                                        }
                                        message += opts;
                                    }
                                }
                                int chunksz = 60 - (keysize + 5);
                                for (size_t i = 0; i < message.length();) { 
                                    char last = *message.substr(i, chunksz).rbegin(); 
                                    if ('\\' == last) {--chunksz;}
                                    outfile << "\"" << message.substr(i, chunksz) << "\""; 
                                    i += chunksz; 
                                    if (i >= message.length()) outfile << "},\n";
                                    else outfile << "\n\t\t";
                                    if (chunksz != 60) chunksz = 60;
                                }
                            }
                        }
                    } 
                } 
                catch (const YAML::BadFile& e) {
                    std::cerr << std::string("Error loading YAML file: ") + e.what() << std::endl;
                } 
                catch (const YAML::Exception& e) {
                    std::cerr << std::string("YAML error: ") + e.what() << std::endl;
                }
            }
        }
    }
    outfile << "};" << std::endl;
    return 0;
}


