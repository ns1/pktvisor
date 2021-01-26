#ifndef PKTVISORD_ABSTRACTMODULE_H
#define PKTVISORD_ABSTRACTMODULE_H

#include <atomic>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>

namespace pktvisor {

class AbstractModule
{
private:
    std::unordered_map<std::string, std::variant<std::string, uint64_t>> _config;
    mutable std::shared_mutex _config_mutex;

protected:
    std::atomic_bool _running = false;
    std::string _name;

public:
    AbstractModule(const std::string &name)
        : _name(name)
    {
    }

    virtual ~AbstractModule(){};

    virtual void start() = 0;
    virtual void stop() = 0;

    const std::string &name() const
    {
        return _name;
    }

    bool running() const
    {
        return _running;
    }

    auto get_config(const std::string &key)
    {
        std::shared_lock lock(_config_mutex);
        return _config[key];
    }

    template <class T>
    void set_config(const std::string &key, const T &val)
    {
        std::unique_lock lock(_config_mutex);
        _config[key] = val;
    }
};

}

#endif //PKTVISORD_ABSTRACTMODULE_H