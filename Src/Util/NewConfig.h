#ifndef INCLUDED_UTIL_CONFIG_H
#define INCLUDED_UTIL_CONFIG_H

#include "Util/Format.h"
#include <map>
#include <memory>
#include <iterator>

namespace Util
{
  namespace Config
  {
    class Node
    {
    public:
      typedef std::shared_ptr<Node> Ptr_t;
      typedef std::shared_ptr<const Node> ConstPtr_t;

    private:
      Ptr_t m_next_sibling;
      Ptr_t m_first_child;
      Ptr_t m_last_child;
      std::map<std::string, Ptr_t> m_children;
      const std::string m_key;  // this cannot be changed (maps depend on it)
      std::string m_value;
      static Node s_empty_node; // key, value, and children must always be empty

      void AddChild(Node &parent, Ptr_t &node);
      Node();                   // prohibit accidental/unintentional creation of blank nodes
      friend Ptr_t CreateEmpty();

    public:
      class const_iterator;

      class iterator: public std::iterator<std::forward_iterator_tag, Node>
      {
      private:
        Ptr_t m_node;
        friend class const_iterator;    
      public:
        inline iterator(Ptr_t node = Ptr_t())
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
        inline Ptr_t operator->() const
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

      class const_iterator: public std::iterator<std::forward_iterator_tag, const Node>
      {
      private:
        ConstPtr_t m_node;
      public:
        inline const_iterator(ConstPtr_t node = ConstPtr_t())
          : m_node(node)
        {}
        inline const_iterator(Ptr_t node = Ptr_t())
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
          iterator current(*this);
          m_node = m_node->m_next_sibling;
          return *this;
        }
        inline const Node &operator*() const
        {
          return *m_node;
        }
        inline ConstPtr_t operator->() const
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

      const inline std::string &ValueWithDefault(const std::string &default_value) const
      {
        if (this == &s_empty_node)
          return default_value;
        return m_value;
      }

      const inline std::string &Value() const
      {
        //TODO: if empty node, throw exception? Use ValueWithDefault() otherwise.
        return m_value;
      }

      bool ValueAsBool() const;
      bool ValueAsBoolWithDefault(bool default_value) const;
      uint64_t ValueAsUnsigned() const;
      uint64_t ValueAsUnsignedWithDefault(uint64_t default_value) const;

      inline void SetValue(const std::string &value)
      {
        if (this != &s_empty_node)  // not allowed to modify empty node
          m_value = value;
      }

      inline bool Empty() const
      {
        return m_key.empty();
      }
      
      inline bool Exists() const
      {
        return !Empty();
      }

      inline bool IsLeaf() const
      {
        return m_children.empty();
      }

      inline bool HasChildren() const
      {
        return !IsLeaf();
      }

      Node &Get(const std::string &path);
      const Node &operator[](const std::string &path) const;
      const Node &Get(const std::string &path) const;
      void Print(size_t indent_level = 0) const;
      std::string ToString(size_t indent_level = 0) const;
      Node &Add(const std::string &key, const std::string &value = "");
      void Set(const std::string &key, const std::string &value);
      Node(const std::string &key);
      Node(const std::string &key, const std::string &value);
      Node(const Node &that);
      ~Node();
    };
    
    //TODO: CreateEmpty() should take a key that defaults to blank
    //TODO: define deep-copy assignment operator and get rid of Ptr_t and ConstPtr_t
    Node::Ptr_t CreateEmpty();
    Node::Ptr_t FromXML(const std::string &text);
    Node::Ptr_t FromXMLFile(const std::string &filename);
    Node::Ptr_t FromINIFile(const std::string &filename);
    Node::Ptr_t MergeINISections(const Node &x, const Node &y);
    void WriteINIFile(const std::string &filename, const Node &config, const std::string &header_comment);
  } // Config
} // Util

#endif // INCLUDED_UTIL_CONFIG_H
