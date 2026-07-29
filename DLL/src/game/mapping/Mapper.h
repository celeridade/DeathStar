#pragma once
#include <string>
#include <unordered_map>

enum GameVersions {
    UNKNOWN = 0,
    BADLION = 2,
    FORGE_1_8 = 4,
    FEATHER_1_8 = 5,
    LUNAR = 7,
    CHEATBREAKER = 8
};

enum MappingType {
    CLASS_MAPPING = 1,
    FIELD_MAPPING = 2,
    METHOD_MAPPING = 3
};

class Mapper {
public:
    static void Initialize(GameVersions version);
    // Zeroes and drops the mapping strings so no trace of them is left in the
    // process heap once the dll unloads.
    static void Clear();
    static std::string Get(const char* mapping, int type = 1);
    static void Set(const char* key, const char* value);
    static void SetMethod(const char* key, const char* value);
    static size_t LoadBadlionMappingsFile(const wchar_t* path);
};
