#include "DIContainer.hpp"

DIContainer* DIContainer::_instance = NULL;

DIContainer::DIContainer() {}

DIContainer& DIContainer::getInstance() {
    if (_instance == NULL)
        _instance = new DIContainer();
    return *_instance;
}

void DIContainer::destroyInstance() {
    if (_instance != NULL) {
        delete _instance;
        _instance = NULL;
    }
}
