// PS3 / PSL1GHT C++ compatibility shims, force-included into every C++ TU via
// the Makefile (-include ps3_compat.h).
//
// The toolchain's libstdc++ (over newlib) does NOT provide std::to_string /
// std::stoi / std::stof (newlib lacks the C99 stdio/stdlib bits libstdc++ keys
// on). The original game uses them, so provide them here. (Adding to namespace
// std is technically UB but standard practice for PS3 homebrew, and safe here
// because these overloads are otherwise absent.)
#pragma once
#ifdef __cplusplus

#include <string>
#include <cstdio>
#include <cstdlib>

namespace std {

inline string to_string(int v)                { char b[32];  snprintf(b, sizeof b, "%d",   v); return b; }
inline string to_string(unsigned v)           { char b[32];  snprintf(b, sizeof b, "%u",   v); return b; }
inline string to_string(long v)               { char b[32];  snprintf(b, sizeof b, "%ld",  v); return b; }
inline string to_string(unsigned long v)      { char b[32];  snprintf(b, sizeof b, "%lu",  v); return b; }
inline string to_string(long long v)          { char b[40];  snprintf(b, sizeof b, "%lld", v); return b; }
inline string to_string(unsigned long long v) { char b[40];  snprintf(b, sizeof b, "%llu", v); return b; }
inline string to_string(float v)              { char b[48];  snprintf(b, sizeof b, "%f",   v); return b; }
inline string to_string(double v)             { char b[48];  snprintf(b, sizeof b, "%f",   v); return b; }

inline int   stoi(const string &s) { return (int)strtol(s.c_str(), nullptr, 10); }
inline long  stol(const string &s) { return strtol(s.c_str(), nullptr, 10); }
inline float stof(const string &s) { return strtof(s.c_str(), nullptr); }
inline double stod(const string &s) { return strtod(s.c_str(), nullptr); }

}  // namespace std

#endif
