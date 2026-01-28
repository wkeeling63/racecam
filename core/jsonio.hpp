#include <fstream>
#include <boost/json.hpp>
namespace json = boost::json;
json::value jsonRead(std::string const&);
void jsonWrite(std::string const&, json::value const&);
void jsonWrite(std::ostream&, json::value const&, std::string* = nullptr);
