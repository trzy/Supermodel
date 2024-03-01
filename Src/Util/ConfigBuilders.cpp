#include "Util/NewConfig.h"
#include "OSD/Logger.h"
#include "Pkgs/tinyxml2.h"
#include <algorithm>
#include <fstream>
#include <queue>
#include <set>
#include <cstdlib>
#include <iostream>

namespace Util
{
  namespace Config
  {
    static void PopulateFromXML(Util::Config::Node *config, const tinyxml2::XMLDocument &xml)
    {
      using namespace tinyxml2;
      std::queue<std::pair<const XMLElement *, Util::Config::Node *>> q;

      // Push the top level of the XML tree
      for (const XMLElement *e = xml.RootElement(); e != 0; e = e->NextSiblingElement())
        q.push( { e, config } );

      // Process the elements in order, pushing subsequent levels along with the
      // config nodes to add them to
      while (!q.empty())
      {
        const XMLElement *element = q.front().first;
        Util::Config::Node *parent_node = q.front().second;
        q.pop();

        // Create a config entry for this XML element
        Util::Config::Node *node = &parent_node->Add(element->Name(), element->GetText() ? std::string(element->GetText()) : std::string());

        // Create entries for each attribute
        for (const XMLAttribute *a = element->FirstAttribute(); a != 0; a = a->Next())
          node->Add(a->Name(), a->Value());

        // Push all child elements
        for (const XMLElement *e = element->FirstChildElement(); e != 0; e = e->NextSiblingElement())
          q.push( { e, node } );
      }
    }

    bool FromXML(Node *config, const std::string &text)
    {
      *config = Node("xml");
      using namespace tinyxml2;
      XMLDocument xml;
      auto ret = xml.Parse(text.c_str());
      if (ret != XML_SUCCESS)
      {
        ErrorLog("Failed to parse XML (%s).", xml.ErrorName());
        return true;
      }
      PopulateFromXML(config, xml);
      return false;
    }

    bool FromXMLFile(Node *config, const std::string &filename)
    {
      *config = Node("xml");
      using namespace tinyxml2;
      XMLDocument xml;
      auto ret = xml.LoadFile(filename.c_str());
      if (ret != XML_SUCCESS)
      {
        ErrorLog("Failed to parse %s (%s).", filename.c_str(), xml.ErrorName());
        return true;
      }
      PopulateFromXML(config, xml);
      return false;
    }

    static std::string StripComment(const std::string &line)
    {
      // Find first semicolon not enclosed in ""
      bool inside_quotes = false;
      for (auto it = line.begin(); it != line.end(); ++it)
      {
        inside_quotes ^= (*it == '\"');
        if (!inside_quotes && *it == ';')
          return std::string(line.begin(), it);
      }
      return line;
    }

    static bool ParseAssignment(Node *current_section, const std::string &filename, size_t line_num, const std::string &line, bool suppress_errors)
    {
      bool parse_error = false;

      size_t idx_equals = line.find_first_of('=');
      if (idx_equals == std::string::npos)
      {
        if (!suppress_errors)
          ErrorLog("%s:%d: Syntax error. No '=' found.", filename.c_str(), line_num);
        return true;
      }
      std::string lvalue(TrimWhiteSpace(std::string(line.begin(), line.begin() + idx_equals)));
      if (lvalue.empty())
      {
        if (!suppress_errors)
          ErrorLog("%s:%d: Syntax error. Setting name missing before '='.", filename.c_str(), line_num);
        return true;
      }
      // Get rvalue, which is allowed to be empty and strip off quotes if they
      // exist. Only a single pair of quotes encasing the rvalue are permitted.
      std::string rvalue(TrimWhiteSpace(std::string(line.begin() + idx_equals + 1, line.end())));
      size_t idx_first_quote = rvalue.find_first_of('\"');
      size_t idx_last_quote = rvalue.find_last_of('\"');
      if (idx_first_quote == 0 && idx_last_quote == (rvalue.length() - 1) && idx_first_quote != idx_last_quote)
        rvalue = std::string(rvalue.begin() + idx_first_quote + 1, rvalue.begin() + idx_last_quote);
      if (std::count(rvalue.begin(), rvalue.end(), '\"') != 0)
      {
        if (!suppress_errors)
          ErrorLog("%s:%d: Warning: Extraneous quotes present on line.", filename.c_str(), line_num);
        parse_error = true;
      }
      // In INI files, we do not allow multiple settings with the same key. If
      // a setting is specified multiple times, previous ones are overwritten.
      current_section->Set(lvalue, rvalue);
      return parse_error;
    }

    static void ParseAssignment(const std::set<Node *> &current_sections, const std::string &filename, size_t line_num, const std::string &line)
    {
      bool suppress_errors = false;
      for (auto section: current_sections)
      {
        suppress_errors |= ParseAssignment(section, filename, line_num, line, suppress_errors);
      }
    }

    bool FromINIFile(Node *config, const std::string &filename)
    {
      std::ifstream file;
      file.open(filename);
      if (file.fail())
      {
        ErrorLog("Unable to open '%s'. Configuration will not be loaded.", filename.c_str());
        return true;
      }

      *config = Node("Global"); // the root is also the "Global" section
      Node &global = *config;
      std::set<Node *> current_sections{ &global }; // default when no section specified

      size_t line_num = 1;
      while (!file.eof())
      {
        if (file.fail())
        {
          ErrorLog("%s:%d: File read error. Configuration will be incomplete.");
          return true;
        }

        std::string line;
        std::getline(file, line);
        line = StripComment(line);
        line = TrimWhiteSpace(line);

        if (!line.empty())
        {
          if (*line.begin() == '[' && *line.rbegin() == ']')
          {
            current_sections.clear();
            std::string sections(line.begin() + 1, line.begin() + line.length() - 1);

            // Handle multiple sections [ a,b,c ] by splitting on ',' and processing individually
            std::vector<std::string> section_list = Util::Format(sections).Split(',');
            for (const std::string &s: section_list)
            {
              // Check if section exists else create a new one
              std::string section = TrimWhiteSpace(s);
              Node *section_node = nullptr;
              if (section.empty() || section == "Global") // empty section names (e.g., "[]") assigned to "[ Global ]"
                section_node = &global;
              else if (!global.TryGet(section))
              {
                Node &new_section = global.Add(section);
                section_node = &new_section;
              }
              else
                section_node = global.TryGet(section);

              // Add to current sections
              current_sections.insert(section_node);
            }
          }
          else
          {
            ParseAssignment(current_sections, filename, line_num, line);
          }
        }

        line_num++;
      }

      return false;
    }

    /*
     * Produces a new INI section by merging two existing sections.
     *
     * - Nodes from x and y with children are ignored as per INI semantics.
     *   The presence of children indicates a section, not a setting.
     * - Settings from y override settings in x if already present.
     * - If multiple settings with the same key are present in either of the
     *   source configs (which technically violates INI semantics), the last
     *   ends up being used.
     * - x's key is retained.
     */
    void MergeINISections(Node *merged, const Node &x, const Node &y)
    {
      *merged = Node(x.Key());  // result has same key as x
      // First copy settings from section x
      for (auto it = x.begin(); it != x.end(); ++it)
      {
        if (it->IsLeaf())
        {
          // INI semantics: take care to only create a single setting per key
          merged->Set(it->Key(), it->GetValue());
        }
      }
      // Merge in settings from section y
      for (auto it = y.begin(); it != y.end(); ++it)
      {
        if (it->IsLeaf() && it->Exists())
          merged->Set(it->Key(), it->GetValue());
      }
    }

    static void WriteSection(std::ofstream &file, const Node &section)
    {
      file << "[ " << section.Key() << " ]" << std::endl;
      file << std::endl;
      for (auto it = section.begin(); it != section.end(); ++it)
      {
        if (it->IsLeaf()) //TODO: check if value exists?
          file << it->Key() << " = " << it->ValueAs<std::string>() << std::endl;
      }
      file << std::endl << std::endl;
    }

    void WriteINIFile(const std::string &filename, const Node &config, const std::string &header_comment)
    {
      std::ofstream file;
      file.open(filename);
      if (file.fail())
      {
        ErrorLog("Unable to write to '%s'. Configuration will not be saved.", filename.c_str());
        return;
      }
      if (!header_comment.empty())
        file << header_comment << std::endl << std::endl;
      WriteSection(file, config);
      for (auto it = config.begin(); it != config.end(); ++it)
      {
        if (it->HasChildren())
          WriteSection(file, *it);
      }
      printf("Configuration successfully saved to '%s'.\n", filename.c_str());
      InfoLog("Configuration successfully saved to '%s'.", filename.c_str());
    }
  }
}
