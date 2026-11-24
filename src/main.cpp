#include "core/GameManager.h"
#include <cstring>

int main(int argc, char* argv[]) {
    bool smokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0) {
            smokeTest = true;
        }
    }
    // Single entry point through the Singleton unified GameManager
    return GameManager::Get().Run(smokeTest);
}
