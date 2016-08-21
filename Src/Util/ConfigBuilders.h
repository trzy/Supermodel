#ifndef INCLUDED_UTIL_CONFIGBUILDERS_H
#define INCLUDED_UTIL_CONFIGBUILDERS_H

#include <string>

namespace Util
{
  namespace Config
  {
    class Node;

    bool FromXML(Node *config, const std::string &text);
    bool FromXMLFile(Node *config, const std::string &filename);
    bool FromINIFile(Node *config, const std::string &filename);
    void MergeINISections(Node *merged, const Node &x, const Node &y);
    void WriteINIFile(const std::string &filename, const Node &config, const std::string &header_comment);
  }
}

#endif  // INCLUDED_UTIL_CONFIGBUILDERS_H
