#include "LiteralsUtils.h"

/******** Utility functions ********/


// The JavaScript representation for a literal with a datatype is
//   "literal"^^http://example.org/datatype
// whereas the HDT representation is
//   "literal"^^<http://example.org/datatype>
// The functions below convert when needed.


// Converts a JavaScript literal to an HDT literal
std::string &toHdtLiteral(std::string &literal) {
    // Check if the object is a literal with a datatype, which needs conversion
    std::string::const_iterator obj;
    std::string::iterator objLast;
    if (*(obj = literal.begin()) == '"' && *(objLast = literal.end() - 1) != '"') {
        // If the start of a datatype was found, surround it with angular brackets
        std::string::const_iterator datatype = objLast;
        while (obj != --datatype && *datatype != '@' && *datatype != '^');
        if (*datatype == '^') {
            // Allocate space for brackets, and update iterators
            literal.resize(literal.length() + 2);
            datatype += (literal.begin() - obj) + 1;
            objLast = literal.end() - 1;
            // Add brackets
            *objLast = '>';
            while (--objLast != datatype)
                *objLast = *(objLast - 1);
            *objLast = '<';
        }
    }
    return literal;
}

// Converts an HDT literal to a JavaScript literal
std::string &fromHdtLiteral(std::string &literal) {
    // Check if the literal has a datatype, which needs conversion
    std::string::const_iterator obj;
    std::string::iterator objLast;
    if (*(obj = literal.begin()) == '"' && *(objLast = literal.end() - 1) == '>') {
        // Find the start of the datatype
        std::string::iterator datatype = objLast;
        while (obj != --datatype && *datatype != '<');
        // Change the datatype representation by removing angular brackets
        if (*datatype == '<')
            literal.erase(datatype), literal.erase(objLast - 1);
    }
    return literal;
}
