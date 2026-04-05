#pragma once

#include <map>

#include "DIToken.hpp"
#include "DIException.hpp"

class DIContainer {
private:
    static DIContainer*  _instance;
    std::map<int, void*> _bindings;

    DIContainer();
    DIContainer(const DIContainer&);
    DIContainer& operator=(const DIContainer&);

public:
    static DIContainer& getInstance();
    static void         destroyInstance();

    template<typename T>
    void bind(DIToken token, T& service) {
        _bindings[static_cast<int>(token)] = static_cast<void*>(&service);
    }

    template<typename T>
    T& resolve(DIToken token) {
        std::map<int, void*>::iterator it = _bindings.find(static_cast<int>(token));
        if (it == _bindings.end())
            throw DIException("DIContainer: service not registered for token");
        return *static_cast<T*>(it->second);
    }
};
