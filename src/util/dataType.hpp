#pragma once

#include <string>

namespace Util {
    enum class DataType {
        Integer,
        Boolean,
        Character,
        IntArray,
        BoolArray,
        CharArray,
        String,
        Void,
        None
    };

    enum class GroupType {
        Primitive,
        Arrays,
        Strings,
        None
    };

    inline DataType getPrimitiveVariant(DataType dataType) {
        switch (dataType) {
            case DataType::IntArray: return DataType::Integer;
            case DataType::BoolArray: return DataType::Boolean;
            case DataType::CharArray: return DataType::Character;
            case DataType::String: return DataType::Character;
            default: return DataType::None;
        }
    }

    inline GroupType getGroupType(DataType dataType) {
        switch (dataType) {
            case DataType::Integer:
            case DataType::Boolean:
            case DataType::Character:
                return GroupType::Primitive;
            case DataType::IntArray:
            case DataType::BoolArray:
            case DataType::CharArray:
                return GroupType::Arrays;
            case DataType::String:
                return GroupType::Strings;
            default:
                return GroupType::None;
        }
    }

    inline std::string dataTypeToString(DataType dataType) {
        switch (dataType) {
            case DataType::Integer: return "int";
            case DataType::Boolean: return "bool";
            case DataType::Character: return "char";
            case DataType::String: return "string";
            case DataType::IntArray: return "int[]";
            case DataType::BoolArray: return "bool[]";
            case DataType::CharArray: return "char[]";
            case DataType::Void: return "void";
            case DataType::None: return "";
            default: return "";
        }
    }
}