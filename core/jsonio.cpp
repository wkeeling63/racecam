#include "jsonio.hpp"
#include <iostream>

json::value jsonRead(std::string const& file)
{
   std::ifstream ifile(file);
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

void jsonWrite(std::string const& file, json::value const& jv)
{
    std::ofstream ofile (file, std::ios::out | std::ios::trunc);
    if (ofile.is_open())
    {
	jsonWrite(ofile, jv);
	ofile.close();
    } else
	throw std::runtime_error("Unable to open configuration file for output! file: " + file);
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
