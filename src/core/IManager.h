#pragma once

// [Requirement 9] Common abstract interface for all feature-specific sub-managers.
// Managed and invoked uniformly by the single Singleton GameManager.
class IManager {
public:
    virtual ~IManager() = default;

    virtual const char* GetName() const = 0;
    virtual bool Init() = 0;
    virtual void Update(float dt) = 0;
    virtual void Render() = 0;
    virtual void Shutdown() = 0;
};
