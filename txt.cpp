#include "txt.h"


void TXT::split(const std::string &src, const std::string &separators, std::vector<std::string> &tokens)
{
    tokens.clear();
    std::size_t i1 = 0;
    std::size_t i2 = src.find_first_of(separators, i1);
    while (i1 < src.length())
    {
        tokens.push_back(src.substr(i1, i2 - i1));
        i1 = src.find_first_not_of(separators, i2);
        i2 = src.find_first_of(separators, i1);
    }
}

void TXT::trim_back(std::string &str)
{
    std::size_t ii = str.find_last_not_of(" \t\r\n");
    if (ii != std::string::npos)
    {
        str.erase(ii + 1);
    }
    else
    {
        str.clear();
    }
}

void TXT::trim_front(std::string &str)
{
    std::size_t ii = str.find_first_not_of(" \t\r\n");
    str.erase(0, ii);
}

bool TXT::substitute(std::string &target, const std::string &placeholder, const std::string &replacement)
{
    bool ret = false;
    std::size_t ii = target.find(placeholder);
    if (ii != std::string::npos)
    {
        target.replace(ii, placeholder.length(), replacement);
        ret = true;
    }
    return ret;
}