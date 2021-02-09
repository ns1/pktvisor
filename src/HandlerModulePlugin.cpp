#include "HandlerModulePlugin.h"
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/FormatStl.h>

namespace vizer {

void HandlerModulePlugin::init_module(InputStreamManager *im,
    HandlerManager *hm, HttpServer &svr)
{
    assert(hm);
    assert(im);
    _input_manager = im;
    _handler_manager = hm;
    _setup_routes(svr);
}
void HandlerModulePlugin::init_module(InputStreamManager *im, HandlerManager *hm)
{
    assert(hm);
    assert(im);
    _input_manager = im;
    _handler_manager = hm;
}

}
