#include <fstream>
#include <boost/json.hpp>

#include <filesystem>
#include <iostream>
#include <pwd.h>    

namespace json = boost::json;

json::value jsonRead(std::string const&, std::string const* = nullptr);
void jsonWrite(std::string const&, json::value const&, std::string const* = nullptr);
void jsonWrite(std::ostream&, json::value const&, std::string* = nullptr);
