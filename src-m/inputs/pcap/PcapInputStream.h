#ifndef PKTVISORD_PCAPINPUTSTREAM_H
#define PKTVISORD_PCAPINPUTSTREAM_H

#include "InputStream.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <IpAddress.h>
#include <PcapLiveDeviceList.h>
#include <TcpReassembly.h>
#include <UdpLayer.h>
#pragma GCC diagnostic pop
#include "utils.h"
#include <functional>
#include <memory>
#include <sigslot/signal.hpp>
#include <unordered_map>
#include <vector>

namespace pktvisor::input::pcap {

enum class PacketDirection {
    toHost,
    fromHost,
    unknown
};

/*
class TcpSessionData final
{
public:
    using malformed_data_cb = std::function<void()>;
    using got_data_cb = std::function<void(std::unique_ptr<char[]> data, size_t size)>;

    TcpSessionData(
        malformed_data_cb malformed_data_handler,
        got_data_cb got_data_handler);

    virtual void receive_data(const char data[], size_t len);

private:
    std::string _buffer;
    malformed_data_cb _malformed_data;
    got_data_cb _got_msg;
};

struct TcpReassemblyData {

    std::shared_ptr<TcpSessionData> _sessionData[2];

    // a flag indicating on which side was the latest message on this connection
    int curSide;
    pcpp::ProtocolType l3Type;

    TcpReassemblyData(bool isIPv4)
    {
        clear();
        (isIPv4) ? l3Type = pcpp::IPv4 : l3Type = pcpp::IPv6;
    }

    ~TcpReassemblyData()
    {
    }

    void clear()
    {
        curSide = -1;
        l3Type = pcpp::UnknownProtocol;
    }
};

struct TcpReassemblyMgr {

    using process_msg_cb = std::function<void(PacketDirection dir, pcpp::ProtocolType l3, uint32_t flowKey, timespec stamp)>;

    process_msg_cb process_msg_handler;

    typedef std::map<uint32_t, TcpReassemblyData> TcpReassemblyConnMgr;

    TcpReassemblyConnMgr connMgr;
};

class TcpMsgReassembly
{
public:
    TcpMsgReassembly(TcpReassemblyMgr::process_msg_cb process_msg_handler);

    std::shared_ptr<pcpp::TcpReassembly> getTcpReassembly()
    {
        return _tcpReassembly;
    }

private:
    TcpReassemblyMgr _reassemblyMgr;
    std::shared_ptr<pcpp::TcpReassembly> _tcpReassembly;
};
*/

class PcapInputStream : public pktvisor::InputStream
{

private:
    IPv4subnetList _hostIPv4;
    IPv6subnetList _hostIPv6;
    pcpp::TcpReassembly _tcpReassembly;
    pcpp::PcapLiveDevice *_pcapDevice;

    bool _pcapFile = false;

protected:
    void _open_pcap(const std::string &fileName, const std::string &bpfFilter = "");
    void _open_iface(const std::string &bpfFilter = "");
    void _get_hosts_from_iface();

public:
    PcapInputStream(const std::string &name);
    ~PcapInputStream();

    // pktvisor::AbstractModule
    void start() override;
    void stop() override;
    json info_json() const override;
    size_t consumer_count() override
    {
        return packet_signal.slot_count() + udp_signal.slot_count() + start_tstamp_signal.slot_count();
    }

    //
    void parse_host_spec();

    // public so it can be called from a static callback method, required by PcapPlusPlus
    void process_raw_packet(pcpp::RawPacket *rawPacket);

    // handler functionality
    sigslot::signal<pcpp::Packet &, PacketDirection, pcpp::ProtocolType, pcpp::ProtocolType, timespec> packet_signal;
    sigslot::signal<pcpp::Packet &, PacketDirection, pcpp::ProtocolType, uint32_t, timespec> udp_signal;
    sigslot::signal<timespec> start_tstamp_signal;
};

}

#endif //PKTVISORD_PCAPINPUTSTREAM_H
