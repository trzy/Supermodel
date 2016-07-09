#include "Util/NewConfig.h"
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
  Util::Config::Node::Ptr_t root = std::make_shared<Util::Config::Node>("global");
  auto &game = root->Add("game");
  game.Add("name", "scud");
  auto &roms = game.Add("roms");
  auto &crom1 = roms.Add("crom");
  crom1.Add("name", "foo.bin");
  crom1.Add("offset", "0");
  auto &crom2 = roms.Add("crom");
  crom2.Add("name", "bar.bin");
  crom2.Add("offset", "2");
  
  const char *expected_output =
    "global  children={ game }\n"
    "  game  children={ name roms }\n"
    "    name=scud  children={ }\n"
    "    roms  children={ crom }\n"
    "      crom  children={ name offset }\n"
    "        name=foo.bin  children={ }\n"
    "        offset=0  children={ }\n"
    "      crom  children={ name offset }\n"
    "        name=bar.bin  children={ }\n"
    "        offset=2  children={ }\n";
  std::cout << "Expected output:" << std::endl << std::endl << expected_output << std::endl;
  std::cout << "Actual config tree:" << std::endl << std::endl;
  root->Print();
  std::cout << std::endl;
  test_results.push_back({ "Manual", root->ToString() == expected_output });

  // Expect second crom: bar.bin
  auto &global = *root;
  std::cout << "game/roms/crom/name=" << global["game/roms/crom/name"].Value() << " (expected: bar.bin)" << std::endl << std::endl;
  test_results.push_back({ "Lookup", global["game/roms/crom/name"].Value() == "bar.bin" });
    
  // Make a copy
  std::cout << "Copy:" << std::endl << std::endl;
  Util::Config::Node::Ptr_t copy = std::make_shared<Util::Config::Node>(*root);
  copy->Print();
  std::cout << std::endl;
  test_results.push_back({ "Copy", copy->ToString() == root->ToString() });
    
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
    "xml  children={ game }\n"
    "  game  children={ name roms }\n"
    "    name=scud  children={ }\n"
    "    roms  children={ region }\n"
    "      region  children={ byte_swap file name stride }\n"
    "        name=crom  children={ }\n"
    "        stride=8  children={ }\n"
    "        byte_swap=true  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=0  children={ }\n"
    "          name=epr-19734.20  children={ }\n"
    "          crc32=0xBE897336  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=2  children={ }\n"
    "          name=epr-19733.19  children={ }\n"
    "          crc32=0x6565E29A  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=4  children={ }\n"
    "          name=epr-19732.18  children={ }\n"
    "          crc32=0x23E864BB  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=6  children={ }\n"
    "          name=epr-19731.17  children={ }\n"
    "          crc32=0x3EE6447E  children={ }\n"
    "      region  children={ byte_swap file name stride }\n"
    "        name=banked_crom  children={ }\n"
    "        stride=8  children={ }\n"
    "        byte_swap=true  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=0x0000000  children={ }\n"
    "          name=mpr-20364.4  children={ }\n"
    "          crc32=0xA2A68EF2  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=0x0000002  children={ }\n"
    "          name=mpr-20363.3  children={ }\n"
    "          crc32=0x3E3CC6FF  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=0x0000004  children={ }\n"
    "          name=mpr-20362.2  children={ }\n"
    "          crc32=0xF7E60DFD  children={ }\n"
    "        file  children={ crc32 name offset }\n"
    "          offset=0x0000006  children={ }\n"
    "          name=mpr-20361.1  children={ }\n"
    "          crc32=0xDDB66C2F  children={ }\n";
  std::cout << "Expected output:" << std::endl << std::endl << expected_xml_config_tree << std::endl;
  std::cout << "Actual config tree:" << std::endl << std::endl;
  Util::Config::Node::Ptr_t xml_config = Util::Config::FromXML(xml);
  xml_config->Print();
  std::cout << std::endl;
  test_results.push_back({ "XML", xml_config->ToString() == expected_xml_config_tree });
  
  // Create a nested key
  {
    Util::Config::Node::Ptr_t config = Util::Config::CreateEmpty();
    config->Add("foo/bar/baz", "bart");
    config->Print();
    std::cout << std::endl;
    test_results.push_back({ "Nested key 1", config->Get("foo/bar/baz").Value() == "bart" });
    config->Get("foo/bar/baz").Set("x", "xx");
    config->Get("foo/bar/baz").Set("y/z", "zz");
    config->Print();
    std::cout << std::endl;
    test_results.push_back({ "Nested key 2", config->Get("foo/bar/baz/x").Value() == "xx" });
    test_results.push_back({ "Nested key 3", config->Get("foo/bar/baz/y/z").Value() == "zz" });
    config->Add("a/b/c");
    config->Get("a/b/c").SetValue("d");
    config->Print();
    std::cout << std::endl;
    test_results.push_back({ "Nested key 4", config->Get("a/b/c").Value() == "d" });
  }
  
  PrintTestResults(test_results);
  return 0;
}
  