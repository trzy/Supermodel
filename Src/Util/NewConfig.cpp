/*
 * Config Tree
 * ===========
 *
 * Accessing Hierarchical Nodes
 * ----------------------------
 *
 * A hierarchical data structure supporting arbitrary nesting. Each node
 * (Config::Node) has a key and either a value or children (in fact, it may
 * have both, but this rarely makes semantic sense and so the config tree
 * builders take care not to allow it).
 *
 * Nesting is denoted with the '/' separator. For example:
 *
 *    1. node.ValueAs<int>()
 *    2. node["foo"]
 *    3. node["foo"].ValueAs<int>()
 *    4. node["foo/bar"]
 *
 * [1] accesses the value of the node (more on this below). [2] accesses the
 * a child node with key "foo". [3] accesses the value of the child node "foo".
 * [4] accesses child "bar" of child "foo", and so forth.
 *
 * Similar to map semantics, the operator [] never fails. If the node does not
 * exist, it creates a dummy "missing" node that is retained as a hidden child.
 * This node will have an empty value, which cannot be accessed, except using
 * the ValueAsDefault<> method. This scheme exists to simplify lookup code for
 * keys known at compile time, the logic being that any "missing" key should
 * could just as well have been there in the first place, thus making the added
 * memory usage negligible. For example, it easily allows values with defaults
 * to be expressed as:
 *
 *    config["foo/bar"].ValueAsDefault<std::string>("default_value")
 *
 * If the lookups are not known at compile time (e.g., are driven by the
 * program user or external data), it is safer to use Get() and TryGet(), which
 * can throw and return nullptr, respectively, and avoid wasting memory with
 * dummy nodes.
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
 *    <game name="scud">
 *    <roms>
 *      <crom name="foo.bin" offset=0/>
 *      <crom name="bar.bin" offset=2/>
 *    </roms>
 *
 *    1. config["game/roms"].ValueAs<std::string>()
 *    2. config["game/roms/crom/offset"].ValueAs<int>()
 *    3. config["game/roms"].begin()
 *
 * [1] will either throw an exception or return an empty string (depending on
 * how the XML was translated to a config tree), [2] will return 2 (the second
 * "crom" tag), and [3] can be used to iterate over both "crom" tags in order.
 *
 * Values
 * ------
 *
 * To check whether or not a value is defined, use the Exists() and Empty()
 * methods.
 *
 * Values are stored in a special container and can be virtually anything.
 * However, it is recommended that only PODs and std::string be used. Arrays
 * and pointers will probably not behave as expected and should be avoided.
 * Trees build from files have all values loaded as std::strings. The types can
 * be changed by subsequent re-assignments or by manually building a tree.
 *
 * To access a value of a given known type, T, use:
 *
 *    node.Value<T>()
 *
 * Conversions as supported but are implemented via serialization and de-
 * serialization using sstream. Most "sane" conversions will work as expected.
 * When a conversion to T is desired, or if the stored value type is not
 * precisely known, use:
 *
 *    node.ValueAs<T>()
 *
 * These functions will throw an exception if the value is not defined at all
 * or if the node does not exist (i.e., a "missing" node from a failed lookup).
 * An alternative that is guaranteed to succeed is:
 *
 *    node.ValueAsDefault<T>(default_value)
 *
 * If the value or node does not exist, default_value is returned, otherwise
 * the stored value is returned with conversion (if needed) to type T.
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
 *    Setting0 = 100
 *    [ Global ]
 *    Setting1 = 200
 *    [ Section1 ]
 *    SettingA = 1      ; this is a comment
 *    SettingB = foo
 *    [ Section2 ]
 *    SettingX = bar
 *    SettingZ = "baz"  ; quotes are optional and are stripped off during parsing
 *
 * Any setting not explicitly part of a section is assigned to the "Global"
 * section. For example, Setting0 and Setting1 above are both part of "Global".
 *
 * Multiple sections can be specified like so:
 *
 *    [ Section1, Section2, Section3 ]
 *    SettingX = foo
 *    [ Section4, , Section5 ]
 *    SettingX = bar
 *
 * In this example, SettingX will be set to "foo" in Section1, Section2, and
 * Section3. It will be set to "bar" in Section4, Section5, and the "Global"
 * section because of the unnamed element.
 *
 * TODO
 * ----
 * - TryGet() can be made quicker by attempting a direct lookup first. We never
 *   add keys with '/' in them (they are split up into nested nodes). Most
 *   lookups are likely to be leaf level, so a direct lookup should be possible
 *   first and, if it fails, the key can be retried as a nested path.
 * - Define our own exceptions?
 */

#include "Util/NewConfig.h"
#include <iostream>

namespace Util
{
  namespace Config
  {
    void Node::CheckEmptyOrMissing() const
    {
      if (m_missing)
        throw std::range_error(Util::Format() << "Node \"" << m_key << "\" does not exist");
      if (Empty())
        throw std::logic_error(Util::Format() << "Node \"" << m_key << "\" has no value" );
    }

    const Node &Node::MissingNode(const std::string &key) const
    {
      auto it = m_missing_nodes.find(key);
      if (it == m_missing_nodes.end())
      {
        auto result = m_missing_nodes.emplace(key, key);
        result.first->second.m_missing = true; // mark this node as missing
        return result.first->second;
      }
      return it->second;
    }

    const Node &Node::operator[](const std::string &path) const
    {
      const Node *e = this;
      std::vector<std::string> keys = Util::Format(path).Split('/');
      for (auto &key: keys)
      {
        auto it = e->m_children.find(key);
        if (it == e->m_children.end())
          return e->MissingNode(key);
        e = it->second.get();
      }
      return *e;
    }

    Node &Node::Get(const std::string &path)
    {
      Node *node = TryGet(path);
      if (!node)
        throw std::range_error(Util::Format() << "Node \"" << path << "\" does not exist");
      return *node;
    }

    const Node &Node::Get(const std::string &path) const
    {
      const Node *node = TryGet(path);
      if (!node)
        throw std::range_error(Util::Format() << "Node \"" << path << "\" does not exist");
      return *node;
    }

    Node *Node::TryGet(const std::string &path)
    {
      Node *e = this;
      std::vector<std::string> keys = Util::Format(path).Split('/');
      for (auto &key: keys)
      {
        auto it = e->m_children.find(key);
        if (it == e->m_children.end())
          return nullptr;
        e = it->second.get();
      }
      return e;
    }

    const Node *Node::TryGet(const std::string &path) const
    {
      const Node *e = this;
      std::vector<std::string> keys = Util::Format(path).Split('/');
      for (auto &key: keys)
      {
        auto it = e->m_children.find(key);
        if (it == e->m_children.end())
          return nullptr;
        e = it->second.get();
      }
      return e;
    }

    void Node::Serialize(std::ostream *os, size_t indent_level) const
    {
      std::fill_n(std::ostream_iterator<char>(*os), 2 * indent_level, ' ');
      *os << m_key << "=\"";
      if (Exists())
        m_value->Serialize(os);
      *os << "\"  children={";
      for (auto v: m_children)
        *os << ' ' << v.first;
      *os << " }" << std::endl;
      for (ptr_t child = m_first_child; child; child = child->m_next_sibling)
        child->Serialize(os, indent_level + 1);
    }

    std::string Node::ToString(size_t indent_level) const
    {
      std::ostringstream os;
      Serialize(&os, indent_level);
      return os.str();
    }

    // Adds an empty node (no value and where Empty() will return true)
    Node &Node::AddEmpty(const std::string &path)
    {
      std::vector<std::string> keys = Util::Format(path).Split('/');
      Node *parent = this;
      ptr_t node;
      for (size_t i = 0; i < keys.size(); i++)
      {
        bool leaf = i == keys.size() - 1;
        auto it = parent->m_children.find(keys[i]);
        if (leaf || it == parent->m_children.end())
        {
          // Create node at this level and leave it empty
          node = std::make_shared<Node>(keys[i]);
          // Attach node to parent and move down to next nesting level: last
          // created node is new parent
          AddChild(*parent, node);
          parent = node.get();
        }
        else
        {
          // Descend deeper...
          parent = it->second.get();
        }
      }
      return *node;
    }

    // Adds a newly-created node (which, among other things, implies no
    // children) as a child
    void Node::AddChild(Node &parent, ptr_t &node)
    {
      if (!parent.m_last_child)
      {
        parent.m_first_child = node;
        parent.m_last_child = node;
      }
      else
      {
        parent.m_last_child->m_next_sibling = node;
        parent.m_last_child = node;
      }
      parent.m_children[node->m_key] = node;
    }

    void Node::DeepCopy(const Node &that)
    {
      if (this == &that)
        return;
      Destroy();
      *const_cast<std::string *>(&m_key) = that.m_key;
      if (that.m_value)
        m_value = that.m_value->MakeCopy();
      for (ptr_t child = that.m_first_child; child; child = child->m_next_sibling)
      {
        ptr_t copied_child = std::make_shared<Node>(*child);
        AddChild(*this, copied_child);
      }
    }

    void Node::Swap(Node &rhs)
    {
      m_next_sibling.swap(rhs.m_next_sibling);
      m_first_child.swap(rhs.m_first_child);
      m_last_child.swap(rhs.m_last_child);
      m_children.swap(rhs.m_children);
     const_cast<std::string *>(&m_key)->swap(*const_cast<std::string *>(&rhs.m_key));
      m_value.swap(rhs.m_value);
    }

    Node &Node::operator=(const Node &rhs)
    {
      DeepCopy(rhs);
      return *this;
    }

    Node &Node::operator=(Node &&rhs) noexcept
    {
      Swap(rhs);
      return *this;
    }

    Node::Node()
      : m_key("config"),  // a default key value
        m_value(nullptr)  // this node is empty
    {
      //std::cout << "<<< Created " << "<null>" << " (" << this << ")" << std::endl;
    }

    Node::Node(const std::string &key)
      : m_key(key),
        m_value(nullptr)  // this node is empty
    {
      //std::cout << "<<< Created " << key << " (" << this << ")" << std::endl;
    }

    Node::Node(const std::string &key, const std::string &value)
      : m_key(key),
        m_value(std::make_shared<ValueInstance<std::string>>(value))
    {
      //std::cout << "<<< Created " << key << '=' << value << " (" << this << ")" << std::endl;
    }

    Node::Node(const Node &that)
    {
      DeepCopy(that);
    }

    Node::Node(Node &&that) noexcept
    {
      Swap(that);
    }

    Node::~Node()
    {
      //std::cout << ">>> Destroyed " << m_key << " (" << this << ")" << std::endl;
    }

    void PrintConfigTree(const Node &config, int indent_level, int tab_stops)
    {
      std::fill_n(std::ostream_iterator<char>(std::cout), tab_stops * indent_level, ' ');
      std::cout << config.Key();
      if (config.Exists())
      {
        std::cout << " = " << config.ValueAs<std::string>();
      }
      std::cout << std::endl;
      for (const Node &child: config)
      {
        PrintConfigTree(child, indent_level + 1, tab_stops);
      }
    }
  } // Config
} // Util
