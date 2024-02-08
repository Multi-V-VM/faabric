#pragma once

#include <faabric/util/exception.h>
#include <string>
#include <vector>
#include <stdint.h>

namespace faabric::util {
std::string readFileToString(const std::string& path);

std::vector<uint8_t> readFileToBytes(const std::string& path);

void writeBytesToFile(const std::string& path,
                      const std::vector<uint8_t>& data);

bool isWasm(const std::vector<uint8_t>& bytes);
}
