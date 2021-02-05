#ifndef PKTVISORD_INPUTSTREAM_H
#define PKTVISORD_INPUTSTREAM_H

#include "AbstractModule.h"
#include "StreamHandler.h"

namespace pktvisor {

class InputStream : public AbstractModule
{

public:
    InputStream(const std::string &name)
        : AbstractModule(name)
    {
    }

    virtual ~InputStream(){};

    virtual size_t consumer_count() = 0;
};

}

#endif //PKTVISORD_INPUTSTREAM_H
