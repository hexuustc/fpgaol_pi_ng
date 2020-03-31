
#include <cstdlib>
#include <string>

#ifndef PROGRAM_HPP
#define PROGRAM_HPP

const std::string DEVICE = "Nexys4DDR";

inline int program_device(const std::string& file_name) {
  std::string cmd = "djtgcfg init -d " + DEVICE + " && djtgcfg prog -d " +
                    DEVICE + " -f " + file_name + " -i 0";
  return system(cmd.data());
}

#endif