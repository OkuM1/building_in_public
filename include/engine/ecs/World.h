#pragma once
#include "Entity.h"
#include "Component.h"
#include <vector>
#include <queue>
#include <bitset>
#include <memory>
#include <cassert>
#include <typeindex>
#include <unordered_map>

namespace engine {

// Interface for component storage (type-erased)
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void entityDestroyed(EntityId entity) = 0;
};

// Concrete storage for a specific component type
template<typename T>
class ComponentArray : public IComponentArray {
public:
    void insertData(EntityId entity, T component) {
        size_t newIndex = size;
        entityToIndexMap[entity] = newIndex;
        indexToEntityMap[newIndex] = entity;
        componentArray[newIndex] = component;
        size++;
    }

    void removeData(EntityId entity) {
        assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Removing non-existent component.");

        // Copy last element into deleted element's place to keep array packed
        size_t indexOfRemovedEntity = entityToIndexMap[entity];
        size_t indexOfLastElement = size - 1;
        
        componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

        // Update map to point to moved spot
        EntityId entityOfLastElement = indexToEntityMap[indexOfLastElement];
        entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
        indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

        entityToIndexMap.erase(entity);
        indexToEntityMap.erase(indexOfLastElement);

        size--;
    }

    T& getData(EntityId entity) {
        assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Retrieving non-existent component.");
        return componentArray[entityToIndexMap[entity]];
    }

    bool hasData(EntityId entity) const {
        return entityToIndexMap.find(entity) != entityToIndexMap.end();
    }

    void entityDestroyed(EntityId entity) override {
        if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
            removeData(entity);
        }
    }

private:
    // Packed array of components
    std::array<T, MAX_ENTITIES> componentArray;
    
    // Map from entity ID to array index
    std::unordered_map<EntityId, size_t> entityToIndexMap;
    
    // Map from array index to entity ID
    std::unordered_map<size_t, EntityId> indexToEntityMap;
    
    size_t size = 0;
};

class World {
public:
    World();
    ~World() = default;

    // Entity Management
    EntityId createEntity();
    void destroyEntity(EntityId entity);
    
    // Component Management
    template<typename T>
    void registerComponent() {
        const char* typeName = typeid(T).name();
        assert(componentTypes.find(typeName) == componentTypes.end() && "Registering component type more than once.");

        componentTypes.insert({typeName, getComponentTypeId<T>()});
        componentArrays.insert({typeName, std::make_shared<ComponentArray<T>>()});
    }

    template<typename T>
    void addComponent(EntityId entity, T component) {
        getComponentArray<T>()->insertData(entity, component);
        
        auto signature = signatures[entity];
        signature.set(getComponentTypeId<T>(), true);
        signatures[entity] = signature;
    }

    template<typename T>
    void removeComponent(EntityId entity) {
        getComponentArray<T>()->removeData(entity);

        auto signature = signatures[entity];
        signature.set(getComponentTypeId<T>(), false);
        signatures[entity] = signature;
    }

    template<typename T>
    T& getComponent(EntityId entity) {
        return getComponentArray<T>()->getData(entity);
    }
    
    template<typename T>
    bool hasComponent(EntityId entity) {
        return getComponentArray<T>()->hasData(entity);
    }

    // Get entity signature (bitset of components)
    const std::bitset<MAX_COMPONENTS>& getSignature(EntityId entity) const {
        return signatures[entity];
    }

private:
    // Entity Manager State
    std::queue<EntityId> availableEntities;
    std::array<std::bitset<MAX_COMPONENTS>, MAX_ENTITIES> signatures;
    uint32_t livingEntityCount = 0;

    // Component Manager State
    std::unordered_map<const char*, std::shared_ptr<IComponentArray>> componentArrays;
    std::unordered_map<const char*, ComponentTypeId> componentTypes;

    template<typename T>
    std::shared_ptr<ComponentArray<T>> getComponentArray() {
        const char* typeName = typeid(T).name();
        assert(componentTypes.find(typeName) != componentTypes.end() && "Component not registered before use.");
        return std::static_pointer_cast<ComponentArray<T>>(componentArrays[typeName]);
    }
};

} // namespace engine
