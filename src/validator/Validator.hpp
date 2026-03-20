#pragma once

#include <vector>
#include <set>

#include "ValidationException.hpp"
#include "../config.hpp" 

class Validator {
private:
    Validator();
    ~Validator();
    Validator(const Validator&);
    Validator& operator=(const Validator&); 

public:
    static void validate(const std::vector<Server>& servers);
};
