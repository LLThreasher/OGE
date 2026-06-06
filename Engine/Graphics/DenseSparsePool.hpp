#pragma once
#include <vector>
#include <cassert>
#include <limits>

using Entity = uint32_t;
static constexpr Entity INVALID_ENTITY = std::numeric_limits<Entity>::max();

template<typename T>
class DenseSparsePool {
public:
    void insert(Entity e, const T& component);
    void remove(Entity e);
    bool contains(Entity e) const;

    T& get(Entity e);
    const T& get(Entity e) const;

    void clear();

    size_t size() const { return dense.size(); }

    const std::vector<Entity>& entities() const { return dense; }
    const std::vector<T>& components() const { return data; }

private:
    std::vector<Entity> dense;
    std::vector<T> data;
    std::vector<size_t> sparse;
};

template<typename T>
void DenseSparsePool<T>::insert(Entity e, const T& component) {
    if (e >= sparse.size()) {
        sparse.resize(e + 1, INVALID_ENTITY);
    }

    assert(!contains(e) && "Entity already has component");

    sparse[e] = dense.size();
    dense.push_back(e);
    data.push_back(component);
}

template<typename T>
bool DenseSparsePool<T>::contains(Entity e) const {
    if (e >= sparse.size())
        return false;

    size_t index = sparse[e];

    if (index == INVALID_ENTITY)
        return false;

    return index < dense.size() && dense[index] == e;
}

template<typename T>
T& DenseSparsePool<T>::get(Entity e) {
    assert(contains(e));
    return data[sparse[e]];
}

template<typename T>
const T& DenseSparsePool<T>::get(Entity e) const {
    assert(contains(e));
    return data[sparse[e]];
}

template<typename T>
void DenseSparsePool<T>::remove(Entity e) {
    assert(contains(e));

    size_t index = sparse[e];
    size_t lastIndex = dense.size() - 1;

    Entity lastEntity = dense[lastIndex];

    // Move last element into removed slot
    dense[index] = lastEntity;
    data[index] = std::move(data[lastIndex]);

    sparse[lastEntity] = index;

    // Pop back
    dense.pop_back();
    data.pop_back();

    sparse[e] = INVALID_ENTITY;
}

template<typename T>
void DenseSparsePool<T>::clear() {
    dense.clear();
    data.clear();
    sparse.clear();
}
