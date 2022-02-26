#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

bool SimpleLRU::deleteNode(lru_node* node) {
    auto prev = node->prev;
    auto next = node->next.get();

    _current_size -= node->key.size() + node->value.size();
    _lru_index.erase(node->key);
    if (!next || !prev) {
        if (!next && !prev) {
            _lru_tail = nullptr;
            _lru_head.reset(nullptr);
        }
        else if (!next) {
            _lru_tail = prev;
            _lru_tail->next = nullptr;
        }
        else {
            _lru_head = std::move(node->next);
            next->prev = nullptr;
        }
    }
    else {
        next->prev = prev;
        prev->next = std::move(node->next);
    }
    return true;
}

void SimpleLRU::freeSpace(lru_node* node = nullptr) {
    while (_current_size > _max_size) {
        if (!node || node->next) {
            deleteNode(_lru_tail);
        }
        else {
            deleteNode(node->prev);
        }
    }
}

void SimpleLRU::putAbsent(const std::string &key, const std::string &value) {
    _current_size += key.size() + value.size();
    freeSpace();

    if (!_lru_head) {
        std::unique_ptr<lru_node> new_node(new lru_node{key, value, nullptr, nullptr});
        _lru_head = std::move(new_node);
        _lru_tail = _lru_head.get();
    }
    else {
        std::unique_ptr<lru_node> new_node(new lru_node{key, value, nullptr, std::move(_lru_head)});
        _lru_head = std::move(new_node);
        _lru_head->next->prev = _lru_head.get();
    }
    _lru_index.emplace(_lru_head->key, *_lru_head);
}

void SimpleLRU::setNode(lru_node* node, const std::string &value) {
    _current_size += value.size() - node->value.size();
    freeSpace(node);
    node->value = value;

    auto prev = node->prev;
    auto next = node->next.get();
    if (!prev) {
        return;
    }
    if (!next) {
        node->next = std::move(_lru_head);
        _lru_head = std::move(prev->next);
        _lru_head->next->prev = _lru_head.get();

        _lru_tail = prev;
    }
    else {
        node->next->prev = prev;
        _lru_tail->next = std::move(_lru_head);

        _lru_head = std::move(prev->next);
        prev->next = std::move(_lru_head->next);
        _lru_head->next = std::move(_lru_tail->next);
        _lru_head->next->prev = _lru_head.get();
    }
    _lru_tail->next = nullptr;
    _lru_head->prev = nullptr;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto node = _lru_index.find(key);
    if (node != _lru_index.end()) {
        setNode(&node->second.get(), value);
    }
    else {
        putAbsent(key, value);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto node = _lru_index.find(key);
    if (node != _lru_index.end()) {
        return false;
    }
    putAbsent(key, value);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto node = _lru_index.find(key);
    if (node != _lru_index.end()) {
        setNode(&node->second.get(), value);
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto node = _lru_index.find(key);
    if (node == _lru_index.end()) {
        return false;
    }
    return deleteNode(&node->second.get());
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto node = _lru_index.find(key);
    if (node == _lru_index.end()) {
        return false;
    }
    value = node->second.get().value;

    setNode(&node->second.get(), value);
    return true;
}

} // namespace Backend
} // namespace Afina
