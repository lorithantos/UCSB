#pragma once

#include <string>

void ConvertFile(std::string const& filename);

// BUGBUG dummy function
inline void ReadFile(std::string const& filename)
{
    ConvertFile(filename);
}

void ReadDirectory();
