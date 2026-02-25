#include <iostream>
#include <memory>

// 简单的测试文件，不依赖于gem5的完整构建系统

class MockMemInterface {
public:
    MockMemInterface() {
        std::cout << "MockMemInterface created" << std::endl;
    }
    virtual ~MockMemInterface() {}
    virtual void init() {}
    virtual void startup() {}
    virtual void wakeup() {}
    virtual bool allRanksDrained() const { return true; }
    virtual long accessLatency() const { return 100; }
    virtual long commandOffset() const { return 4; }
};

class MockNVMObject {
public:
    MockNVMObject() {
        std::cout << "MockNVMObject created" << std::endl;
    }
    virtual ~MockNVMObject() {}
    virtual bool RequestComplete(void *req) { return true; }
    virtual void Cycle(long cycles) {}
};

class NVMainMemInterface : public MockMemInterface, public MockNVMObject {
public:
    NVMainMemInterface() {
        std::cout << "NVMainMemInterface created" << std::endl;
    }
    ~NVMainMemInterface() {
        std::cout << "NVMainMemInterface destroyed" << std::endl;
    }
    void init() override {
        std::cout << "NVMainMemInterface::init() called" << std::endl;
    }
    void startup() override {
        std::cout << "NVMainMemInterface::startup() called" << std::endl;
    }
    void wakeup() override {
        std::cout << "NVMainMemInterface::wakeup() called" << std::endl;
    }
    bool allRanksDrained() const override {
        std::cout << "NVMainMemInterface::allRanksDrained() called" << std::endl;
        return true;
    }
    long accessLatency() const override {
        std::cout << "NVMainMemInterface::accessLatency() called" << std::endl;
        return 200;
    }
    long commandOffset() const override {
        std::cout << "NVMainMemInterface::commandOffset() called" << std::endl;
        return 8;
    }
};

int main() {
    std::cout << "Testing NVMainMemInterface..." << std::endl;

    // Create NVMainMemInterface instance
    NVMainMemInterface *nvmain = new NVMainMemInterface();

    // Test init method
    std::cout << "\nCalling init()..." << std::endl;
    nvmain->init();

    // Test startup method
    std::cout << "\nCalling startup()..." << std::endl;
    nvmain->startup();

    // Test wakeup method
    std::cout << "\nCalling wakeup()..." << std::endl;
    nvmain->wakeup();

    // Test accessLatency method
    std::cout << "\nCalling accessLatency()..." << std::endl;
    long latency = nvmain->accessLatency();
    std::cout << "accessLatency returned: " << latency << std::endl;

    // Test commandOffset method
    std::cout << "\nCalling commandOffset()..." << std::endl;
    long offset = nvmain->commandOffset();
    std::cout << "commandOffset returned: " << offset << std::endl;

    // Test allRanksDrained method
    std::cout << "\nCalling allRanksDrained()..." << std::endl;
    bool drained = nvmain->allRanksDrained();
    std::cout << "allRanksDrained returned: " << (drained ? "true" : "false") << std::endl;

    // Clean up
    delete nvmain;

    std::cout << "\nTest completed!" << std::endl;
    return 0;
}
