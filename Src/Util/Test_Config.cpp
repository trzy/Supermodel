#include "Util/NewConfig.h"
#include <iostream>

int main()
{
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
  auto &game = root->Create("game");
  game.Create("name", "scud");
  auto &roms = game.Create("roms");
  auto &crom1 = roms.Create("crom");
  crom1.Create("name", "foo.bin");
  crom1.Create("offset", "0");
  auto &crom2 = roms.Create("crom");
  crom2.Create("name", "bar.bin");
  crom2.Create("offset", "2");
  
  const char *expected_output =
    "global  children={ game }              \n"
    "  game  children={ name roms }         \n"
    "    name=scud  children={ }            \n"
    "    roms  children={ crom }            \n"
    "      crom  children={ name offset }   \n"
    "        name=foo.bin  children={ }     \n"
    "        offset=0  children={ }         \n"
    "      crom  children={ name offset }   \n"
    "        name=bar.bin  children={ }     \n"
    "        offset=2  children={ }         \n";
  std::cout << "Expected output:" << std::endl << std::endl << expected_output << std::endl;
  std::cout << "Actual config tree:" << std::endl << std::endl;
  root->Print();
  std::cout << std::endl;

  // Expect second crom: bar.bin
  auto &global = *root;
  std::cout << "game/roms/crom/name=" << global["game/roms/crom/name"].Value() << " (expected: bar.bin)" << std::endl << std::endl;
    
  // Make a copy
  std::cout << "Copy:" << std::endl << std::endl;
  Util::Config::Node::Ptr_t copy = std::make_shared<Util::Config::Node>(*root);
  copy->Print();
  std::cout << std::endl;
    
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
    "xml  children={ game }                                 \n"
    "  game  children={ name roms }                         \n"
    "    name=scud  children={ }                            \n"
    "    roms  children={ region }                          \n"
    "      region  children={ byte_swap file name stride }  \n"
    "        name=crom  children={ }                        \n"
    "        stride=8  children={ }                         \n"
    "        byte_swap=true  children={ }                   \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=0  children={ }                       \n"
    "          name=epr-19734.20  children={ }              \n"
    "          crc32=0xBE897336  children={ }               \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=2  children={ }                       \n"
    "          name=epr-19733.19  children={ }              \n"
    "          crc32=0x6565E29A  children={ }               \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=4  children={ }                       \n"
    "          name=epr-19732.18  children={ }              \n"
    "          crc32=0x23E864BB  children={ }               \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=6  children={ }                       \n"
    "          name=epr-19731.17  children={ }              \n"
    "          crc32=0x3EE6447E  children={ }               \n"
    "      region  children={ byte_swap file name stride }  \n"
    "        name=banked_crom  children={ }                 \n"
    "        stride=8  children={ }                         \n"
    "        byte_swap=true  children={ }                   \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=0x0000000  children={ }               \n"
    "          name=mpr-20364.4  children={ }               \n"
    "          crc32=0xA2A68EF2  children={ }               \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=0x0000002  children={ }               \n"
    "          name=mpr-20363.3  children={ }               \n"
    "          crc32=0x3E3CC6FF  children={ }               \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=0x0000004  children={ }               \n"
    "          name=mpr-20362.2  children={ }               \n"
    "          crc32=0xF7E60DFD  children={ }               \n"
    "        file  children={ crc32 name offset }           \n"
    "          offset=0x0000006  children={ }               \n"
    "          name=mpr-20361.1  children={ }               \n"
    "          crc32=0xDDB66C2F  children={ }               \n";
  std::cout << "Expected output:" << std::endl << std::endl << expected_xml_config_tree << std::endl;
  std::cout << "Actual config tree:" << std::endl << std::endl;
  Util::Config::Node::Ptr_t xml_config = Util::Config::FromXML(xml);
  xml_config->Print();
  std::cout << std::endl;
  return 0;
}
  