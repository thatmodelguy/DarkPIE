#pragma once

#include "Ast.h"

#include <vector>

// Catches misspelled functions and devices that still parse as plain names
// (KeeppDown, Microphone.Sahid, Keey.Esc, and the like).
std::vector<ParseError> checkScriptNames(const Program& program);
