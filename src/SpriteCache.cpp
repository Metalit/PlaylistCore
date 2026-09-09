#include "SpriteCache.hpp"

#include <map>

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/UI/Image.hpp"

using namespace UnityEngine;

std::unordered_map<std::string_view, Sprite*> strings;
std::unordered_map<Sprite*, std::string> sprites;

void CacheSprite(Sprite* sprite, std::string base64) {
    if (!strings.contains(base64)) {
        sprite->hideFlags = UnityEngine::HideFlags::DontSave;
        sprites.emplace(sprite, std::move(base64));
        strings.emplace(sprites[sprite], sprite);
    }
}

Sprite* HasCachedSprite(std::string_view base64) {
    auto findIter = strings.find(base64);
    if (findIter != strings.end())
        return findIter->second;
    return nullptr;
}

void RemoveCachedSprite(Sprite* sprite) {
    auto findIter = sprites.find(sprite);
    if (findIter != sprites.end()) {
        strings.erase(strings.find(findIter->second));
        sprites.erase(findIter);
    }
    sprite->hideFlags = UnityEngine::HideFlags::None;
    Object::Destroy(sprite);
}

void ClearCachedSprites() {
    for (auto& [sprite, _] : sprites) {
        sprite->hideFlags = UnityEngine::HideFlags::None;
        Object::Destroy(sprite);
    }
    strings.clear();
    sprites.clear();
}
