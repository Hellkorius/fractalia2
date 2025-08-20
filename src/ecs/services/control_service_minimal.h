#pragma once

#include "../core/service_locator.h"

// Minimal test service
class TestControlService {
public:
    DECLARE_SERVICE(TestControlService);
    
    TestControlService() {}
    ~TestControlService() {}
    
    bool initialize() { return true; }
    void cleanup() {}
};