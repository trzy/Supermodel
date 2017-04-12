#include "Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include <iostream>

static void PrintTestResults(std::vector<std::pair<std::string, bool>> results)
{
  std::cout << "TEST RESULTS" << std::endl;
  std::cout << "------------" << std::endl;
  for (auto v: results)
    std::cout << v.first << ": " << (v.second ? "passed" : "FAILED") << std::endl;
}

int main()
{
  std::vector<std::pair<std::string, bool>> test_results;

  /*
   * Creates a config tree corresponding to the following:
   *
   *  <game>
   *    <name>scud</name>
   *    <roms>
   *      <crom>
   *        <name>foo.bin</name>
   *        <offset>0</offset>
   *      </crom>
   *      <crom>
   *        <name>bar.bin</name>
   *        <offset>2</offset>
   *      </crom>
   *    </roms>
   *  </game>
   */
  Util::Config::Node root("global");
  auto &game = root.Add("game");
  game.Add<std::string>("name", "scud");
  auto &roms = game.Add("roms");
  auto &crom1 = roms.Add("crom");
  crom1.Add<std::string>("name", "foo.bin");
  crom1.Add<std::string>("offset", "0");
  auto &crom2 = roms.Add("crom");
  crom2.Add<std::string>("name", "bar.bin");
  crom2.Add<std::string>("offset", "2");
  
  const char *expected_output =
    "global=\"\"  children={ game }\n"
    "  game=\"\"  children={ name roms }\n"
    "    name=\"scud\"  children={ }\n"
    "    roms=\"\"  children={ crom }\n"
    "      crom=\"\"  children={ name offset }\n"
    "        name=\"foo.bin\"  children={ }\n"
    "        offset=\"0\"  children={ }\n"
    "      crom=\"\"  children={ name offset }\n"
    "        name=\"bar.bin\"  children={ }\n"
    "        offset=\"2\"  children={ }\n";
  std::cout << "Expected output:" << std::endl << std::endl << expected_output << std::endl;
  std::cout << "Actual config tree:" << std::endl << std::endl;
  root.Serialize(&std::cout);
  std::cout << std::endl;
  test_results.push_back({ "Manual", root.ToString() == expected_output });

  // Expect second crom: bar.bin
  std::cout << "game/roms/crom/name=" << root["game/roms/crom/name"].Value<std::string>() << " (expected: bar.bin)" << std::endl << std::endl;
  test_results.push_back({ "Lookup", root["game/roms/crom/name"].Value<std::string>() == "bar.bin" });
    
  // Make a copy using copy constructor
  std::cout << "Copy constructed:" << std::endl << std::endl;
  Util::Config::Node copy(root);
  copy.Serialize(&std::cout);
  std::cout << std::endl;
  test_results.push_back({ "Copy constructed", copy.ToString() == root.ToString() });
  
  // Make a deep copy using assignment operator, check equality, then modify a
  // value and ensure it does not change in the copy
  std::cout << "Copy by assignment:" << std::endl << std::endl;
  Util::Config::Node copy2("foo", "bar");
  copy2.Add<std::string>("x/y/z", "test");
  copy2.Add<std::string>("dummy", "value");
  std::cout << "Initially set to:" << std::endl;
  copy2.Serialize(&std::cout);
  copy2 = root;
  std::cout << "Copy of \"global\":" << std::endl;
  copy2.Serialize(&std::cout);
  std::cout << std::endl;
  bool copies_are_equivalent = copy2.ToString() == root.ToString();
  copy2.Get("game/name").SetValue("vf3");
  bool copies_are_unique = copy2["game/name"].Value<std::string>() == "vf3" && root["game/name"].Value<std::string>() == "scud";
  test_results.push_back({ "Copy by assignment", copies_are_equivalent && copies_are_unique });

  // Parse from XML
  const char *xml =
    "<game name=\"scud\">                                                           \n"
    "  <roms>                                                                       \n"
    "    <region name=\"crom\" stride=\"8\" byte_swap=\"true\">                     \n"
    "      <file offset=\"0\" name=\"epr-19734.20\" crc32=\"0xBE897336\" />         \n"
    "      <file offset=\"2\" name=\"epr-19733.19\" crc32=\"0x6565E29A\" />         \n"
    "      <file offset=\"4\" name=\"epr-19732.18\" crc32=\"0x23E864BB\" />         \n"
    "      <file offset=\"6\" name=\"epr-19731.17\" crc32=\"0x3EE6447E\" />         \n"
    "    </region>                                                                  \n"
    "    <region name=\"banked_crom\" stride=\"8\" byte_swap=\"true\">              \n"
    "      <file offset=\"0x0000000\" name=\"mpr-20364.4\" crc32=\"0xA2A68EF2\" />  \n"
    "      <file offset=\"0x0000002\" name=\"mpr-20363.3\" crc32=\"0x3E3CC6FF\" />  \n"
    "      <file offset=\"0x0000004\" name=\"mpr-20362.2\" crc32=\"0xF7E60DFD\" />  \n"
    "      <file offset=\"0x0000006\" name=\"mpr-20361.1\" crc32=\"0xDDB66C2F\" />  \n"
    "    </region>                                                                  \n"
    "  </roms>                                                                      \n"
    "</game>                                                                        \n";
  const char *expected_xml_config_tree =
    "xml=\"\"  children={ game }\n"
    "  game=\"\"  children={ name roms }\n"
    "    name=\"scud\"  children={ }\n"
    "    roms=\"\"  children={ region }\n"
    "      region=\"\"  children={ byte_swap file name stride }\n"
    "        name=\"crom\"  children={ }\n"
    "        stride=\"8\"  children={ }\n"
    "        byte_swap=\"true\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"0\"  children={ }\n"
    "          name=\"epr-19734.20\"  children={ }\n"
    "          crc32=\"0xBE897336\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"2\"  children={ }\n"
    "          name=\"epr-19733.19\"  children={ }\n"
    "          crc32=\"0x6565E29A\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"4\"  children={ }\n"
    "          name=\"epr-19732.18\"  children={ }\n"
    "          crc32=\"0x23E864BB\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"6\"  children={ }\n"
    "          name=\"epr-19731.17\"  children={ }\n"
    "          crc32=\"0x3EE6447E\"  children={ }\n"
    "      region=\"\"  children={ byte_swap file name stride }\n"
    "        name=\"banked_crom\"  children={ }\n"
    "        stride=\"8\"  children={ }\n"
    "        byte_swap=\"true\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"0x0000000\"  children={ }\n"
    "          name=\"mpr-20364.4\"  children={ }\n"
    "          crc32=\"0xA2A68EF2\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"0x0000002\"  children={ }\n"
    "          name=\"mpr-20363.3\"  children={ }\n"
    "          crc32=\"0x3E3CC6FF\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"0x0000004\"  children={ }\n"
    "          name=\"mpr-20362.2\"  children={ }\n"
    "          crc32=\"0xF7E60DFD\"  children={ }\n"
    "        file=\"\"  children={ crc32 name offset }\n"
    "          offset=\"0x0000006\"  children={ }\n"
    "          name=\"mpr-20361.1\"  children={ }\n"
    "          crc32=\"0xDDB66C2F\"  children={ }\n";
  std::cout << "Expected output:" << std::endl << std::endl << expected_xml_config_tree << std::endl;
  std::cout << "Actual config tree:" << std::endl << std::endl;
  Util::Config::Node xml_config("xml");
  Util::Config::FromXML(&xml_config, xml);
  xml_config.Serialize(&std::cout);
  std::cout << std::endl;
  test_results.push_back({ "XML", xml_config.ToString() == expected_xml_config_tree });
  
  // Create a nested key
  {
    Util::Config::Node config("global");
    config.Add<std::string>("foo/bar/baz", "bart");
    config.Serialize(&std::cout);
    std::cout << std::endl;
    test_results.push_back({ "Nested key 1", config["foo/bar/baz"].Value<std::string>() == "bart" });
    config.Get("foo/bar/baz").Set<std::string>("x", "xx");
    config.Get("foo/bar/baz").Set<std::string>("y/z", "zz");
    config.Serialize(&std::cout);
    std::cout << std::endl;
    test_results.push_back({ "Nested key 2", config["foo/bar/baz/x"].Value<std::string>() == "xx" });
    test_results.push_back({ "Nested key 3", config["foo/bar/baz/y/z"].Value<std::string>() == "zz" });
    config.Add("a/b/c");
    config.Get("a/b/c").SetValue<std::string>("d");
    config.Serialize(&std::cout);
    std::cout << std::endl;
    test_results.push_back({ "Nested key 4", config["a/b/c"].Value<std::string>() == "d" });
  }
  
  // Multiple Set() calls should only create duplicates of the leaf node
  {
    const char *expected_config =
      "global=\"\"  children={ a foo x }\n"
      "  foo=\"\"  children={ bar }\n"
      "    bar=\"\"  children={ }\n"
      "    bar=\"\"  children={ }\n"
      "  x=\"\"  children={ }\n"
      "  x=\"\"  children={ }\n"
      "  a=\"\"  children={ b }\n"
      "    b=\"\"  children={ c }\n"
      "      c=\"\"  children={ }\n"
      "      c=\"\"  children={ }\n";
    Util::Config::Node config("global");
    config.Add("foo/bar");
    config.Add("foo/bar");
    config.Add("x");
    config.Add("x");
    config.Add("a/b/c");
    config.Add("a/b/c");
    config.Serialize(&std::cout);
    test_results.push_back({ "Duplicate leaf nodes", config.ToString() == expected_config });
  }

  PrintTestResults(test_results);
  return 0;
}
  