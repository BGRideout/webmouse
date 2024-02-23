#ifndef TXT_H
#define TXT_H

#include <string>
#include <vector>

class TXT
{
public:
    static void split(const std::string &src, const std::string &separators, std::vector<std::string> &tokens);
    static void trim_back(std::string &str);
    static void trim_front(std::string &str);

};

#endif