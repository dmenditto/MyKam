#pragma once

#include <string>
#include <vector>

// Small locale-independent text helpers shared by the slot store and the config parser.

namespace mykam {

// Remove leading and trailing spaces, tabs, CR and LF.
std::string trim(const std::string& s);

// Split text into lines on LF, CRLF or CR. A trailing newline does not produce an empty last line.
std::vector<std::string> splitLines(const std::string& text);

// Parse a whole string as a finite decimal number (surrounding blanks allowed).
// Returns false for empty text, trailing junk, NaN and infinity.
bool parseDouble(const std::string& s, double& out);

// Shortest text that reads back to exactly the same double.
std::string formatDouble(double value);

// Fixed notation with at most `decimals` places and no trailing zeros ("1.5", "-3", "0.1235").
// Used for slot files so they stay small whatever noise the floating point values carry.
std::string formatCompact(double value, int decimals);

}  // namespace mykam
