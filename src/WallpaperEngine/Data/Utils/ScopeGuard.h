#pragma once
#include <utility>

namespace WallpaperEngine::Data::Utils {
/**
 * Simple basic class that runs code on scope exit
 */
template <typename F>
struct ScopeGuard {
    ScopeGuard(const ScopeGuard& other) = delete;
    ScopeGuard& operator=(const ScopeGuard& other) = delete;

    explicit ScopeGuard (F&& f) : func(std::forward<F>(f)), owner(true) {}
    explicit ScopeGuard(ScopeGuard&& other) noexcept : func(std::move(other.func)), owner(other.owner) {
        other.owner = false;
    }
    ~ScopeGuard() {
        this->execute();
    }

    /**
     * Prevents the guard from being run on scope exit
     */
    void cancel() {
        this->owner = false;
    }

protected:
    void execute() {
        if (this->owner) {
            this->func();
        }

        this->owner = false;
    }

    F func;
    bool owner;
};
};