#ifndef OSTRICH_LITERALSUTILS_H
#define OSTRICH_LITERALSUTILS_H

#include <string>

// Converts a JavaScript literal to an HDT literal
std::string &toHdtLiteral(std::string &literal);

// Converts an HDT literal to a JavaScript literal
std::string &fromHdtLiteral(std::string &literal);

#endif //OSTRICH_LITERALSUTILS_H
