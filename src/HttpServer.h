/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <httplib.h>
#include <spdlog/spdlog.h>

namespace vizer {
class HttpServer : public httplib::Server
{
    bool _read_only = true;

public:
    HttpServer(bool read_only)
        : _read_only(read_only)
    {
    }

    Server &Get(const char *pattern, Handler handler)
    {
        spdlog::info("Registering GET {}", pattern);
        return httplib::Server::Get(pattern, handler);
    }
    Server &Post(const char *pattern, Handler handler)
    {
        if (_read_only) {
            return *this;
        }
        spdlog::info("Registering POST {}", pattern);
        return httplib::Server::Post(pattern, handler);
    }
    Server &Put(const char *pattern, Handler handler)
    {
        if (_read_only) {
            return *this;
        }
        spdlog::info("Registering PUT {}", pattern);
        return httplib::Server::Put(pattern, handler);
    }
    Server &Delete(const char *pattern, Handler handler)
    {
        if (_read_only) {
            return *this;
        }
        spdlog::info("Registering DELETE {}", pattern);
        return httplib::Server::Delete(pattern, handler);
    }
};
}
