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
  return 0;
}
  