#include "PcapInputModulePlugin.h"
#include <Corrade/PluginManager/AbstractManager.h>
#include <Corrade/Utility/FormatStl.h>

CORRADE_PLUGIN_REGISTER(VizerInputPcap, vizer::input::pcap::PcapInputModulePlugin,
    "dev.vizer.module.input/1.0")

namespace vizer::input::pcap {

void PcapInputModulePlugin::_setup_routes(HttpServer &svr)
{

    // CREATE
    svr.Post("/api/v1/inputs/pcap", std::bind(&PcapInputModulePlugin::_create, this, std::placeholders::_1, std::placeholders::_2));

    // DELETE
    svr.Delete("/api/v1/inputs/pcap/(\\w+)", std::bind(&PcapInputModulePlugin::_delete, this, std::placeholders::_1, std::placeholders::_2));

    // GET
    svr.Get("/api/v1/inputs/pcap/(\\w+)", std::bind(&PcapInputModulePlugin::_read, this, std::placeholders::_1, std::placeholders::_2));
}

void PcapInputModulePlugin::_create(const httplib::Request &req, httplib::Response &res)
{
    json result;
    try {
        auto body = json::parse(req.body);
        std::unordered_map<std::string, std::string> schema = {
            {"name", "\\w+"},
            {"iface", "\\w+"}};
        std::unordered_map<std::string, std::string> opt_schema = {
            {"pcap_source", "[_a-z]+"}};
        try {
            _check_schema(body, schema, opt_schema);
        } catch (const SchemaException &e) {
            res.status = 400;
            result["error"] = e.what();
            res.set_content(result.dump(), "text/json");
            return;
        }
        if (_input_manager->module_exists(body["name"])) {
            res.status = 400;
            result["error"] = "input name already exists";
            res.set_content(result.dump(), "text/json");
            return;
        }
        std::string bpf;
        if (body.contains("bpf")) {
            bpf = body["bpf"];
        }

        {
            auto input_stream = std::make_unique<PcapInputStream>(body["name"]);
            input_stream->config_set("iface", body["iface"].get<std::string>());
            input_stream->config_set("bpf", bpf);
            if (body.contains("pcap_source")) {
                input_stream->config_set("pcap_source", body["pcap_source"].get<std::string>());
            }
            _input_manager->module_add(std::move(input_stream));
            // the module is now started and owned by the manager
        }

        auto [input_stream, stream_mgr_lock] = _input_manager->module_get_locked(body["name"]);
        assert(input_stream);
        input_stream->info_json(result);
        res.set_content(result.dump(), "text/json");
    } catch (const std::exception &e) {
        res.status = 500;
        result["error"] = e.what();
        res.set_content(result.dump(), "text/json");
    }
}
void PcapInputModulePlugin::_read(const httplib::Request &req, httplib::Response &res)
{
    json result;
    try {
        auto name = req.matches[1];
        if (!_input_manager->module_exists(name)) {
            res.status = 404;
            result["error"] = "input name does not exist";
            res.set_content(result.dump(), "text/json");
            return;
        }
        auto [input_stream, stream_mgr_lock] = _input_manager->module_get_locked(name);
        assert(input_stream);
        input_stream->info_json(result);
        res.set_content(result.dump(), "text/json");
    } catch (const std::exception &e) {
        res.status = 500;
        result["error"] = e.what();
        res.set_content(result.dump(), "text/json");
    }
}
void PcapInputModulePlugin::_delete(const httplib::Request &req, httplib::Response &res)
{
    json result;
    try {
        auto name = req.matches[1];
        if (!_input_manager->module_exists(name)) {
            res.status = 404;
            result["error"] = "input name does not exist";
            res.set_content(result.dump(), "text/json");
            return;
        }
        auto [input_stream, stream_mgr_lock] = _input_manager->module_get_locked(name);
        assert(input_stream);
        auto count = input_stream->consumer_count();
        if (count) {
            res.status = 400;
            result["error"] = Corrade::Utility::formatString("input stream has existing consumers ({}), remove them first", count);
            res.set_content(result.dump(), "text/json");
            return;
        }
        // manually unlock so we can remove
        stream_mgr_lock.unlock();
        _input_manager->module_remove(name);
        res.set_content(result.dump(), "text/json");
    } catch (const std::exception &e) {
        res.status = 500;
        result["error"] = e.what();
        res.set_content(result.dump(), "text/json");
    }
}

}
