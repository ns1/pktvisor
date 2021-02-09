#ifndef VIZERD_NETHANDLERMODULEPLUGIN_H
#define VIZERD_NETHANDLERMODULEPLUGIN_H

#include "HandlerModulePlugin.h"

namespace vizer::handler::net {

class NetHandlerModulePlugin : public HandlerModulePlugin
{

protected:
    void _setup_routes(HttpServer &svr) override;

public:
    explicit NetHandlerModulePlugin(Corrade::PluginManager::AbstractManager &manager, const std::string &plugin)
        : vizer::HandlerModulePlugin{manager, plugin}
    {
    }

    std::string name() const override
    {
        return "NetHandler";
    }
};
}

#endif //VIZERD_NETHANDLERMODULEPLUGIN_H
