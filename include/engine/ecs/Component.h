#pragma once
#include <cstdint>
#include <cstddef>

namespace engine {

// Unique ID generator for component types
using ComponentTypeId = uint32_t;

namespace internal {
    inline ComponentTypeId getUniqueComponentId() {
        static ComponentTypeId lastId = 0;
        return lastId++;
    }
}

// Template function to get a unique ID for each component type T
template <typename T>
inline ComponentTypeId getComponentTypeId() {
    static ComponentTypeId typeId = internal::getUniqueComponentId();
    return typeId;
}

// Maximum number of component types supported
constexpr size_t MAX_COMPONENTS = 32;

} // namespace engine
