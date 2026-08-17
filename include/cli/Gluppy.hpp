#pragma once

#include "config/AppConfig.hpp"
#include <string>
#include <vector>

namespace cli {

class Gluppy {
public:
  enum class ParseResult { SUCCESS, HELP_REQUESTED, ERROR };

  Gluppy(int argc, char *argv[]);

  ParseResult Parse(config::AppConfig &config);
  void PrintHelp(const std::string &binary_name) const;

private:
  bool HasFlag(const std::string &flag, const std::string &alt = "") const;
  std::string GetOption(const std::string &flag, const std::string &alt = "",
                        const std::string &default_val = "") const;

  std::vector<std::string> args_;
};

} // namespace cli
