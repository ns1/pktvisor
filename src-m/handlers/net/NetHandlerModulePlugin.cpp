#include "NetHandlerModulePlugin.h"
#include <Corrade/PluginManager/AbstractManager.h>
#include <json/json.hpp>

using json = nlohmann::json;

CORRADE_PLUGIN_REGISTER(NetHandler, pktvisor::handler::NetHandlerModulePlugin,
    "com.ns1.module.handler/1.0")

namespace pktvisor {
namespace handler {

void NetHandlerModulePlugin::_setup_routes(HttpServer &svr)
{
    // CREATE
    svr.Post("/api/v1/inputs/pcap/(\\w+)/handlers/net", [this](const httplib::Request &req, httplib::Response &res) {
        json result;
        try {
            auto body = json::parse(req.body);
            std::unordered_map<std::string, std::string> schema = {
                {"name", "\\w+"}};
            try {
                _check_schema(body, schema);
            } catch (const SchemaException &e) {
                res.status = 400;
                result["error"] = e.what();
                res.set_content(result.dump(), "text/json");
                return;
            }
            auto input_name = req.matches[1];
            if (!_input_manager->exists(input_name)) {
                res.status = 404;
                result["error"] = "input name does not exist";
                res.set_content(result.dump(), "text/json");
                return;
            }
            if (_handler_manager->exists(body["name"])) {
                res.status = 400;
                result["error"] = "handler name already exists";
                res.set_content(result.dump(), "text/json");
                return;
            }
            // note, may be a race on exists() above, this may fail. if so we will catch and 500.
            auto [input_stream, stream_mgr_lock] = _input_manager->get_module(input_name);
            assert(input_stream);
            auto pcap_stream = dynamic_cast<PcapInputStream *>(input_stream);
            if (!pcap_stream) {
                res.status = 400;
                result["error"] = "input stream is not pcap";
                res.set_content(result.dump(), "text/json");
                return;
            }
            if (!input_stream->running()) {
                res.status = 400;
                result["error"] = "input stream is not running";
                res.set_content(result.dump(), "text/json");
                return;
            }
            auto handler_module = std::make_unique<NetStreamHandler>(body["name"], pcap_stream);
            _handler_manager->add_module(body["name"], std::move(handler_module));
            result["name"] = body["name"];
            res.set_content(result.dump(), "text/json");
        } catch (const std::exception &e) {
            res.status = 500;
            result["error"] = e.what();
            res.set_content(result.dump(), "text/json");
        }
    });
    svr.Get("/api/v1/inputs/pcap/(\\w+)/handlers/net/(\\w+)", [this](const httplib::Request &req, httplib::Response &res) {
        json result;
        try {
            auto input_name = req.matches[1];
            if (!_input_manager->exists(input_name)) {
                res.status = 404;
                result["result"] = "input name does not exist";
                res.set_content(result.dump(), "text/json");
                return;
            }
            auto handler_name = req.matches[2];
            if (!_handler_manager->exists(handler_name)) {
                res.status = 404;
                result["result"] = "handler name does not exist";
                res.set_content(result.dump(), "text/json");
                return;
            }
            auto [handler, handler_mgr_lock] = _handler_manager->get_module(handler_name);
            auto net_handler = dynamic_cast<pktvisor::handler::NetStreamHandler *>(handler);
            if (!net_handler) {
                res.status = 400;
                result["error"] = "handler stream is not net";
                res.set_content(result.dump(), "text/json");
                return;
            }
            net_handler->toJSON(result, 0, false);
            res.set_content(result.dump(), "text/json");
        } catch (const std::exception &e) {
            res.status = 500;
            result["result"] = e.what();
            res.set_content(result.dump(), "text/json");
        }
    });
    // DELETE
    svr.Delete("/api/v1/inputs/pcap/(\\w+)/handlers/net/(\\w+)", [this](const httplib::Request &req, httplib::Response &res) {
        json result;
        try {
            auto input_name = req.matches[1];
            if (!_input_manager->exists(input_name)) {
                res.status = 404;
                result["result"] = "input name does not exist";
                res.set_content(result.dump(), "text/json");
                return;
            }
            auto handler_name = req.matches[2];
            if (!_handler_manager->exists(handler_name)) {
                res.status = 404;
                result["result"] = "handler name does not exist";
                res.set_content(result.dump(), "text/json");
                return;
            }
            _handler_manager->remove_module(handler_name);
            res.set_content(result.dump(), "text/json");
        } catch (const std::exception &e) {
            res.status = 500;
            result["result"] = e.what();
            res.set_content(result.dump(), "text/json");
        }
    });
}

}
}