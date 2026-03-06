/* 
 * jsonio.hpp json read/write header
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

#include <fstream>
#include <boost/json.hpp>

#include <filesystem>
#include <iostream>
#include <pwd.h>    

namespace json = boost::json;

json::value jsonRead(std::string const&, std::string const* = nullptr);
void jsonWrite(std::string const&, json::value const&, std::string const* = nullptr);
void jsonWrite(std::ostream&, json::value const&, std::string* = nullptr);
