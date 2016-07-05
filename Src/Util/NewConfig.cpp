/*
 * Config Tree
 * -----------
 *
 * A hierarchical data structure supporting arbitrary nesting. Each node
 * (Config::Node) has a key and either a value or children (in fact, it may
 * have both, but this does not make semantic sense and so the config tree 
 * builders take care not to allow it).
 *
 * All values are simply strings for now. They may have defaults, which are 
 * actually supplied by the caller and returned when a queried node cannot be
 * found.
 *
 * Nodes at the same nesting level (siblings) are strung together in a
 * linked list. Parents also maintain pointers to the first and last of their
 * children (for order-preserving iteration) as well as a map for direct 
 * lookup by key.
 *
 * Keys may be reused at a given level. Key-value pairs will have their order
 * preserved when iterated but only the most recent key is returned in direct
 * map lookups.
 *
 * Example:
 *
 *  <game name="scud">
 *  <roms>
 *    <crom name="foo.bin" offset=0/>
 *    <crom name="bar.bin" offset=2/>
 *  </roms>
 *
 * config["game/roms"].Value()              <-- should return nothing
 * config["game/roms/crom/offset"].Value()  <-- "2", the second <crom> tag
 * config["game/roms"].begin()              <-- iterate over two <crom> tags
 *
 * INI Semantics
 * -------------
 *
 * INI files are linear in nature and divided into sections. Config trees built
 * to represent INI files must adhere to the following rules:
 *
 *  - Section nodes have their key set to the section name and value empty.
 *    They are the only nodes that can have children (i.e., IsLeaf() == false,
 *    HasChildren() == true).
 *  - Top-level node in the tree is the global section, and its key is 
 *    "Global".
 *  - Only the global section (top-level section) may have child nodes that are
 *    sections. The config tree can therefore only be up to 2 levels deep: the
 *    first, top-most level consists of global parameters, and section nodes
 *    provide one more level of nesting.
 *
 * INI files contain zero or more sections followed by zero or more settings
 * each.
 *
 *  Setting0 = 100
 *  [ Global ]
 *  Setting1 = 200
 *  [ Section1 ]
 *  SettingA = 1      ; this is a comment
 *  SettingB = foo
 *  [ Section2 ]
 *  SettingX = bar
 *  SettingZ = "baz"  ; quotes are optional and are stripped off during parsing
 *
 * Any setting not explicitly part of a section is assigned to the "Global"
 * section. For example, Setting0 and Setting1 above are both part of "Global".
 */

#include "Util/NewConfig.h"
#include "OSD/Logger.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace Util
{
  namespace Config
  {
    Node Node::s_empty_node;

    const Node &Node::operator[](const std::string &path) const
    {
      const Node *e = this;
      std::vector<std::string> keys = Util::Format(path).Split('/');
      for (auto &key: keys)
      {
        auto it = e->m_children.find(key);
        if (it == e->m_children.end())
          return s_empty_node;
        e = it->second.get();
      }
      return *e;
    }

    Node &Node::Get(const std::string &path)
    {
      return const_cast<Node &>(operator[](path));
    }

    const Node &Node::Get(const std::string &path) const
    {
      return operator[](path);
    }

    void Node::Print(size_t indent_level) const
    {
      std::fill_n(std::ostream_iterator<char>(std::cout), 2 * indent_level, ' ');
      std::cout << m_key;
      if (m_value.length())
        std::cout << '=' << m_value;
      std::cout << "  children={";
      for (auto v: m_children)
        std::cout << ' ' << v.first;
      std::cout << " }" << std::endl;
      for (Ptr_t child = m_first_child; child; child = child->m_next_sibling)
        child->Print(indent_level + 1);
    }

    Node &Node::Create(const std::string &key)
    {
      Ptr_t node = std::make_shared<Node>(key);
      AddChild(node);
      return *node;
    }

    Node &Node::Create(const std::string &key, const std::string &value)
    {
      Ptr_t node = std::make_shared<Node>(key, value);
      AddChild(node);
      return *node;
    }

    // Adds a newly-created node (which, among other things, implies no
    // children) as a child 
    void Node::AddChild(Ptr_t &node)
    {
      if (!m_last_child)
      {
        m_first_child = node;
        m_last_child = node;
      }
      else
      {
        m_last_child->m_next_sibling = node;
        m_last_child = node;
      }    
      m_children[node->m_key] = node;
    }

    /*
    void Node::Clear()
    {
      m_next_sibling.reset();
      m_first_child.reset();
      m_last_child.reset();
      m_children.clear();
      m_key.clear();
      m_value.clear();
    }
    */

    Node::Node()
    {
      //std::cout << "Created " << "<null>" << " (" << this << ")" << std::endl;
    }

    Node::Node(const std::string &key)
      : m_key(key)
    {
      //std::cout << "Created " << key << " (" << this << ")" << std::endl;
    }

    Node::Node(const std::string &key, const std::string &value)
      : m_key(key),
        m_value(value)
    {
      //std::cout << "Created " << key << '=' << value << " (" << this << ")" << std::endl;
    }

    // Deep copy
    Node::Node(const Node &that)
      : m_key(that.m_key),
        m_value(that.m_value)
    {
      for (Ptr_t child = that.m_first_child; child; child = child->m_next_sibling)
      {
        Ptr_t copied_child = std::make_shared<Node>(*child);
        AddChild(copied_child);
      }
    }

    Node::~Node()
    {
      //std::cout << "Destroyed " << m_key << " (" << this << ")" << std::endl;
    }

    Node::Ptr_t CreateEmpty()
    {
      return std::shared_ptr<Node>(new Node());
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

    static void ParseAssignment(Node &current_section, const std::string &filename, size_t line_num, const std::string &line)
    {
      size_t idx_equals = line.find_first_of('=');
      if (idx_equals == std::string::npos)
      {
        //std::cerr << filename << ':' << line_num << ": Syntax error. No '=' found." << std::endl;
        ErrorLog("%s:%d: Syntax error. No '=' found.", filename.c_str(), line_num);
        return;
      }
      std::string lvalue(TrimWhiteSpace(std::string(line.begin(), line.begin() + idx_equals)));
      if (lvalue.empty())
      {
        //std::cerr << filename << ':' << line_num << ": Syntax error. Setting name missing before '='." << std::endl;
        ErrorLog("%s:%d: Syntax error. Setting name missing before '='.", filename.c_str(), line_num);
        return;
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
        //std::cerr << filename << ':' << line_num << ": Warning: Extraneous quotes present on line." << std::endl;
        ErrorLog("%s:%d: Warning: Extraneous quotes present on line.", filename.c_str(), line_num);
      }
      // In INI files, we do not allow multiple settings with the same key. If
      // a setting is specified multiple times, previous ones are overwritten.
      if (current_section[lvalue].Empty())
        current_section.Create(lvalue, rvalue);
      else
        current_section.Get(lvalue).SetValue(rvalue);
    }
          
    Node::Ptr_t FromINIFile(const std::string &filename)
    {
      std::ifstream file;
      file.open(filename);
      if (file.fail())
      {
        ErrorLog("Unable to open '%s'. Configuration will not be loaded.", filename.c_str());
        return CreateEmpty();
      }

      Node::Ptr_t global_ptr = std::make_shared<Node>("Global");  // the root is also the "Global" section
      Node &global = *global_ptr;
      Node *current_section = &global;

      size_t line_num = 1;
      while (!file.eof())
      {
        if (file.fail())
        {
          ErrorLog("%s:%d: File read error. Configuration will be incomplete.");
          return CreateEmpty();
        }

        std::string line;
        std::getline(file, line);
        line = StripComment(line);
        line = TrimWhiteSpace(line);

        if (!line.empty())
        {
          if (*line.begin() == '[' && *line.rbegin() == ']')
          {
            // Check if section exists else create a new one
            std::string section(TrimWhiteSpace(std::string(line.begin() + 1, line.begin() + line.length() - 1)));
            if (section.empty() || section == "Global") // empty section names (e.g., "[]") assigned to "[ Global ]"
              current_section = &global;
            else if (global[section].Empty())
            {
              Node &new_section = global.Create(section);
              current_section = &new_section;
            }
            else
              current_section = &global.Get(section);
          }
          else
          {
            ParseAssignment(*current_section, filename, line_num, line);
          }
        }

        line_num++;
      }

      return global_ptr;
    }
    
    /*
     * Produces a new INI section by merging two existing sections.
     *
     * - Nodes from x and y with children are ignored as per INI semantics.
     *   The presence of children indicates a section, not a setting.
     * - Settings from y override settings in x if already present.
     * - x's key is retained.
     */
    Node::Ptr_t MergeINISections(const Node &x, const Node &y)
    {
      Node::Ptr_t merged_ptr = std::make_shared<Node>(x.Key()); // result has same key as x
      Node &merged = *merged_ptr;
      // First copy settings from section x
      for (auto it = x.begin(); it != x.end(); ++it)
      {
        if (it->IsLeaf())
          merged.Create(it->Key(), it->Value());
      }
      // Merge in settings from section y
      for (auto it = y.begin(); it != y.end(); ++it)
      {
        auto &key = it->Key();
        auto &value = it->Value();
        if (it->IsLeaf())
        {
          if (merged[key].Empty())
            merged.Create(key, value);
          else
            merged.Get(key).SetValue(value);
        }
      }
      return merged_ptr;
    }

    static void WriteSection(std::ofstream &file, const Node &section)
    {
      file << "[ " << section.Key() << " ]" << std::endl;
      file << std::endl;
      for (auto it = section.begin(); it != section.end(); ++it)
      {
        if (it->IsLeaf())
          file << it->Key() << " = \"" << it->Value() << "\"" << std::endl;
      }
      file << std::endl << std::endl;
    }

    void WriteINIFile(const std::string &filename, const Node &config, const std::string &header_comment)
    {
      std::ofstream file;
      file.open(filename);
      if (file.fail())
      {
        ErrorLog("Unable to write to '%s'. Configuration will not be saved.");
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
    }
  } // Config
} // Util

