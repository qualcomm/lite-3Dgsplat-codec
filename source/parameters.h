/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "LGSCMisc.h"

struct Parameters {
  bool isDecoder = false;

  // output mode for ply writing (binary or ascii)
  bool outputBinaryPly = true;

  std::string uncompressedDataPath;
  std::string compressedStreamPath;
  std::string reconstructedDataPath;

  NumBits numBits;
  
  int sphericalOrder = 3;
  int compLevel = 2;
};

// ------------- argutil -------------
namespace argutil {

inline std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

inline bool starts_with(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

inline bool parse_bool(const std::string& v, bool& out) {
  std::string s;
  s.reserve(v.size());
  for (char c : v) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  if (s == "1" || s == "true" || s == "yes" || s == "on")  { out = true;  return true; }
  if (s == "0" || s == "false"|| s == "no"  || s == "off") { out = false; return true; }
  return false;
}

inline bool parse_int(const std::string& v, int& out) {
  try {
    size_t idx = 0;
    int val = std::stoi(v, &idx);
    if (idx != v.size()) return false;
    out = val;
    return true;
  } catch (...) {
    return false;
  }
}

inline std::unordered_map<std::string, std::string> load_config_file(const std::string& path, std::ostream& warn_out) {
  std::unordered_map<std::string, std::string> kv;
  std::ifstream f(path);
  if (!f) {
    warn_out << "Warning: cannot open config file: " << path << "\n";
    return kv;
  }
  std::string line;
  size_t line_no = 0;
  while (std::getline(f, line)) {
    ++line_no;
    std::string t = trim(line);
    if (t.empty() || t[0] == '#' || t[0] == ';')
      continue;
    auto pos = t.find('=');
    if (pos == std::string::npos) {
      warn_out << "Warning: ignoring malformed config line " << line_no << ": " << t << "\n";
      continue;
    }
    std::string key = trim(t.substr(0, pos));
    std::string val = trim(t.substr(pos + 1));
    kv[key] = val;
  }
  return kv;
}

struct ParsedCli {
  std::unordered_map<std::string, std::string> kv;   // --key -> value
  std::unordered_set<std::string> flags;             // --flag without value (e.g., --help)
  std::vector<std::string> unhandled;                // unknown tokens
};

// Accepts: --key=value, --key value, -c value, --help/-h
inline ParsedCli parse_argv(int argc, char* argv[]) {
  ParsedCli out;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      out.flags.insert("help");
      continue;
    }

    if (arg == "--config" || arg == "-c") {
      if (i + 1 < argc) {
        out.kv["config"] = argv[++i];
      } else {
        out.unhandled.push_back(arg); // missing value
      }
      continue;
    }

    if (starts_with(arg, "--")) {
      auto pos = arg.find('=');
      if (pos != std::string::npos) {
        std::string key = arg.substr(2, pos - 2);
        std::string val = arg.substr(pos + 1);
        out.kv[key] = val;
      } else {
        // try to read next token as value if present and not another switch
        if (i + 1 < argc && !(starts_with(argv[i + 1], "-"))) {
          out.kv[arg.substr(2)] = argv[++i];
        } else {
          // key-only flag (e.g., --foo) -> mark as flag
          out.flags.insert(arg.substr(2));
        }
      }
      continue;
    }

    // short alias like -c or -h handled above; any other token is unhandled
    out.unhandled.push_back(arg);
  }
  return out;
}

inline void print_help(std::ostream& os) {
    os <<
    "Usage: app [options]\n"
    "\n"
    "Options:\n"
    "  --help, -h                    Show this help text\n"
    "  --config <file>, -c <file>    Configuration file (key=value per line)\n"
    "\n"
    "General:\n"
    "  --mode <0|1>                  Encoding/decoding mode\n"
    "                                0: encode\n"
    "                                1: decode\n"
    "\n"
    "IO:\n"
    "  --reconstructedDataPath <path>   Output reconstructed file (decoder only)\n"
    "  --uncompressedDataPath <path>    Input file path\n"
    "  --compressedStreamPath <path>    Bitstream path (encoder=output, decoder=input)\n"
    "\n"
    "Format:\n"
    "  --outputBinaryPly <bool>      Output PLY as binary (true/false).\n"
    "\n"
    "Codec params:\n"
    "  --sphericalOrder <0|1|2|3>    Order of spherical harmonics (default: 3)\n"
    "  --compLevel <0|1|2>           Compression level (higher = more compression; default: 2)\n"
    "\n"
    "Examples:\n"
    "  app --mode 0 --uncompressedDataPath in.ply --compressedStreamPath out.lgsc\n"
    "  app -c config.cfg --mode 1 --compressedStreamPath in.lgsc --reconstructedDataPath out.ply\n";
}
} // namespace argutil


static inline void apply_string_fn(const std::unordered_map<std::string, std::string>& src,
                                    const char* key, std::string& dst)
{
  std::unordered_map<std::string, std::string>::const_iterator it = src.find(key);
  if (it != src.end()) dst = it->second;
}

static inline void apply_bool_fn(const std::unordered_map<std::string, std::string>& src,
                                const char* key, bool& dst, std::ostream& warn)
{
  std::unordered_map<std::string, std::string>::const_iterator it = src.find(key);
  if (it != src.end()) {
    bool val;
    if (!argutil::parse_bool(it->second, val)) {
      warn << "Warning: invalid boolean for '" << key << "': " << it->second << "\n";
    } else {
      dst = val;
    }
  }
}

static inline void apply_int_range_fn(const std::unordered_map<std::string, std::string>& src,
                                     const char* key, int& dst, int lo, int hi, std::ostream& warn)
{
  std::unordered_map<std::string, std::string>::const_iterator it = src.find(key);
  if (it != src.end()) {
    int v;
    if (!argutil::parse_int(it->second, v)) {
      warn << "Warning: invalid integer for '" << key << "': " << it->second << "\n";
    } else if (v < lo || v > hi) {
      warn << "Warning: '" << key << "' out of range [" << lo << "," << hi << "]: " << v << "\n";
    } else {
      dst = v;
    }
  }
}


inline bool ParseParameters(int argc, char* argv[], Parameters& params) {
  using namespace argutil;
 
  // Parse CLI
  ParsedCli cli = parse_argv(argc, argv);

  // Help or no args -> print help and stop (false)
  if (argc == 1 || cli.flags.count("help")) {
    print_help(std::cout);
    return false;
  }

  // Load config if provided
  std::unordered_map<std::string, std::string> cfg;
  {
    std::unordered_map<std::string, std::string>::const_iterator it = cli.kv.find("config");
    if (it != cli.kv.end()) {
      cfg = load_config_file(it->second, std::cerr);
    }
  }

  // Apply config first (overrides defaults)
  {
    int mode_tmp = params.isDecoder ? 1 : 0;
    apply_int_range_fn(cfg, "sphericalOrder", params.sphericalOrder, 0, 3, std::cerr);
    apply_int_range_fn(cfg, "compLevel",      params.compLevel,      0, 2, std::cerr);
    apply_int_range_fn(cfg, "mode",           mode_tmp,              0, 1, std::cerr);
    params.isDecoder = (mode_tmp == 1);

    apply_string_fn(cfg, "reconstructedDataPath", params.reconstructedDataPath);
    apply_string_fn(cfg, "uncompressedDataPath",  params.uncompressedDataPath);
    apply_string_fn(cfg, "compressedStreamPath",  params.compressedStreamPath);
    apply_bool_fn  (cfg, "outputBinaryPly",       params.outputBinaryPly, std::cerr);
  }

  // Apply CLI last (overrides config)
  {
    int mode_tmp = params.isDecoder ? 1 : 0;
    apply_int_range_fn(cli.kv, "sphericalOrder", params.sphericalOrder, 0, 3, std::cerr);
    apply_int_range_fn(cli.kv, "compLevel",      params.compLevel,      0, 2, std::cerr);
    apply_int_range_fn(cli.kv, "mode",           mode_tmp,              0, 1, std::cerr);
    params.isDecoder = (mode_tmp == 1);

    apply_string_fn(cli.kv, "reconstructedDataPath", params.reconstructedDataPath);
    apply_string_fn(cli.kv, "uncompressedDataPath",  params.uncompressedDataPath);
    apply_string_fn(cli.kv, "compressedStreamPath",  params.compressedStreamPath);
    apply_bool_fn  (cli.kv, "outputBinaryPly",       params.outputBinaryPly, std::cerr);
  }

  // Unhandled tokens
  for (std::vector<std::string>::const_iterator it = cli.unhandled.begin();
       it != cli.unhandled.end(); ++it) {
    std::cerr << "Warning: unhandled argument ignored: " << *it << "\n";
  }

  return true;
}

//----------------------------------------------------------------------------

inline void setEncoderParams(CoderParams& coderParams, const Parameters& params) {
  coderParams.sphericalOrder = params.sphericalOrder;
  coderParams.uncompressedDataPath = params.uncompressedDataPath;
  coderParams.compressedStreamPath = params.compressedStreamPath;
}
inline void setDecoderParmas(DecoderParams& decoderParams, const Parameters& params) {
  decoderParams.compressedStreamPath = params.compressedStreamPath;
  decoderParams.reconstructedDataPath = params.reconstructedDataPath;
  decoderParams.outputBinaryPly = params.outputBinaryPly;
}

#endif