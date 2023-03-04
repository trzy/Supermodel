#ifndef INCLUDED_UTIL_CONFIG_H
#define INCLUDED_UTIL_CONFIG_H

#include "Util/GenericValue.h"
#include <map>
#include <memory>
#include <iterator>
#include <exception>

namespace Util
{
  namespace Config
  {
    class Node
    {
    private:
      typedef std::shared_ptr<Node> ptr_t;
      typedef std::shared_ptr<const Node> const_ptr_t;
      const std::string m_key;                // this cannot be changed (maps depend on it)
      std::shared_ptr<GenericValue> m_value;  // null pointer marks this node as an invalid, empty node
      ptr_t m_next_sibling;
      ptr_t m_first_child;
      ptr_t m_last_child;
      std::map<std::string, ptr_t> m_children;
      mutable std::map<std::string, Node> m_missing_nodes;  // missing nodes from failed queries (must also be empty)
      bool m_missing = false;

      void Destroy()
      {
        m_value.reset();
        m_next_sibling.reset();
        m_first_child.reset();
        m_last_child.reset();
        m_children.clear();
      }

      void CheckEmptyOrMissing() const;
      const Node &MissingNode(const std::string &key) const;
      Node &AddEmpty(const std::string &path);
      void AddChild(Node &parent, ptr_t &node);
      void DeepCopy(const Node &that);
      void Swap(Node &rhs);
      Node(); // prohibit accidental/unintentional creation of empty nodes

    public:
      class const_iterator;

      class iterator
      {
      private:
        ptr_t m_node;
        friend class const_iterator;
      public:
        inline iterator(ptr_t node = ptr_t())
          : m_node(node)
        {}
        inline iterator(const iterator &it)
          : m_node(it.m_node)
        {}
        inline iterator operator++()
        {
          // Prefix increment
          m_node = m_node->m_next_sibling;
          return *this;
        }
        inline iterator operator++(int)
        {
          // Postfix increment
          iterator current(*this);
          m_node = m_node->m_next_sibling;
          return *this;
        }
        inline Node &operator*() const
        {
          return *m_node;
        }
        inline ptr_t operator->() const
        {
          return m_node;
        }
        inline bool operator==(const iterator &rhs) const
        {
          return m_node == rhs.m_node;
        }
        inline bool operator!=(const iterator &rhs) const
        {
          return m_node != rhs.m_node;
        }
      };

      class const_iterator
      {
      private:
        const_ptr_t m_node;
      public:
        inline const_iterator()
		  : m_node(const_ptr_t())
        {}
        inline const_iterator(const_ptr_t node)
          : m_node(node)
        {}
        inline const_iterator(ptr_t node)
          : m_node(std::const_pointer_cast<const Node>(node))
        {}
        inline const_iterator(const const_iterator &it)
          : m_node(it.m_node)
        {}
        inline const_iterator(const Node::iterator &it)
          : m_node(it.m_node)
        {}
        inline const_iterator operator++()
        {
          // Prefix increment
          m_node = m_node->m_next_sibling;
          return *this;
        }
        inline const_iterator operator++(int)
        {
          // Postfix increment
          //iterator current(*this);	//unreferenced local variable
          m_node = m_node->m_next_sibling;
          return *this;
        }
        inline const Node &operator*() const
        {
          return *m_node;
        }
        inline const_ptr_t operator->() const
        {
          return m_node;
        }
        inline bool operator==(const const_iterator &rhs) const
        {
          return m_node == rhs.m_node;
        }
        inline bool operator!=(const const_iterator &rhs) const
        {
          return m_node != rhs.m_node;
        }
      };

      inline iterator begin()
      {
        return iterator(m_first_child);
      }

      inline iterator end()
      {
        return iterator();
      }

      inline const_iterator begin() const
      {
        return iterator(m_first_child);
      }

      inline const_iterator end() const
      {
        return iterator();
      }

      const inline std::string &Key() const
      {
        return m_key;
      }

      inline std::shared_ptr<GenericValue> GetValue() const
      {
        return m_value;
      }

      inline void Clear()
      {
        m_value = nullptr;
      }

      inline void SetValue(const std::shared_ptr<GenericValue> &value)
      {
        m_value = value;
      }

      template <typename T>
      const T &Value() const
      {
        CheckEmptyOrMissing();
        return m_value->Value<T>();
      }

      template <typename T>
      T ValueAs() const
      {
        CheckEmptyOrMissing();
        return m_value->ValueAs<T>();
      }

      template <typename T>
      T ValueAsDefault(const T &default_value) const
      {
        if (Empty())
          return default_value;
        return m_value->ValueAs<T>();
      }

      // Set value of this node
      template <typename T>
      inline void SetValue(const T &value)
      {
        if (!m_missing)
        {
          if (!Empty() && m_value->Is<T>())
            m_value->Set(value);
          else
            m_value = std::make_shared<ValueInstance<T>>(value);
        }
        else
          throw std::range_error(Util::Format() << "Node \"" << m_key << "\" does not exist");
      }

      // char* is a troublesome case because we want to convert it to std::string
      inline void SetValue(const char *value)
      {
        SetValue<std::string>(value);
      }

      // Add a child node. Multiple nodes of the same key may be added but only
      // when specified as leaves. For example, adding "foo/bar" twice will
      // result in one "foo" with two "bar" children.
      template <typename T>
      Node &Add(const std::string &path, const T &value)
      {
        Node &new_leaf_node = AddEmpty(path);
        new_leaf_node.SetValue(value);
        return new_leaf_node;
      }

      Node &Add(const std::string &path)
      {
        return Add(path, std::string());
      }

      // Set the value of the matching child node if it exists, else add new
      template <typename T>
      void Set(const std::string &key, const T &value)
      {
        Node *node = TryGet(key);
        if (node)
          node->SetValue(value);
        else
          Add(key, value);
      }

      void SetEmpty(const std::string &key)
      {
        Node *node = TryGet(key);
        if (node)
        {
          node->Clear();
        }
        else
        {
          AddEmpty(key);
        }
      }

      // True if value is empty (does not exist)
      inline bool Empty() const
      {
        return !m_value;
      }

      // True if value exists (is not empty)
      inline bool Exists() const
      {
        return !Empty();
      }

      // True if no keys under this node
      inline bool IsLeaf() const
      {
        return m_children.empty();
      }

      // True if this node has keys
      inline bool HasChildren() const
      {
        return !IsLeaf();
      }

      // Always succeeds -- failed lookups permanently create an empty node.
      // Use with caution. Intended for hard-coded lookups.
      const Node &operator[](const std::string &path) const;

      // These throw if the node is missing
      Node &Get(const std::string &path);
      const Node &Get(const std::string &path) const;

      // This returns nullptr if node is missing
      Node *TryGet(const std::string &path);
      const Node *TryGet(const std::string &path) const;

      void Serialize(std::ostream *os, size_t indent_level = 0) const;
      std::string ToString(size_t indent_level = 0) const;
      Node &operator=(const Node &rhs);
      Node& operator=(Node&& rhs) noexcept;
      Node(const std::string &key);
      Node(const std::string &key, const std::string &value);
      Node(const Node &that);
      Node(Node&& that) noexcept;
      ~Node();
    };

    void PrintConfigTree(const Node &config, int indent_level = 0, int tab_stops = 2);
  } // Config
} // Util

#endif // INCLUDED_UTIL_CONFIG_H
