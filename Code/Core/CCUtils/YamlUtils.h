#ifndef YAMLUTILS_H
#define YAMLUTILS_H

#include <rapidyaml-0.10.0.hpp>
#include <string>

namespace CC
{
    // Reading a yaml node's value or key.
    //
    // A node holds a view into the parsed buffer rather than a string, so
    // every caller needs the same conversion. These live here so that adding a
    // second thing that reads yaml does not add a second copy of them.

    inline std::string NodeToString(ryml::ConstNodeRef node)
    {
        std::string result;
        if (node.has_val())
        {
            c4::csubstr value = node.val();
            result = std::string(value.data(), value.size());
        }
        return result;
    }

    inline std::string KeyToString(ryml::ConstNodeRef node)
    {
        std::string result;
        if (node.has_key())
        {
            c4::csubstr key = node.key();
            result = std::string(key.data(), key.size());
        }
        return result;
    }

    // Extracted by the parser rather than through a string conversion, which
    // avoids an allocation and the locale sensitivity of the C conversions.
    inline float NodeToFloat(ryml::ConstNodeRef node)
    {
        float result = 0.0f;
        if (node.has_val())
        {
            node >> result;
        }
        return result;
    }
}

#endif // YAMLUTILS_H
