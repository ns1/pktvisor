#include "DnsStreamHandler.h"
#include "GeoDB.h"
#include "utils.h"
#include <Corrade/Utility/Debug.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <IPv4Layer.h>
#include <IPv6Layer.h>
#pragma GCC diagnostic pop
#include <arpa/inet.h>
#include <datasketches/datasketches/cpc/cpc_union.hpp>
#include <sstream>

namespace pktvisor::handler::dns {

DnsStreamHandler::DnsStreamHandler(const std::string &name, PcapInputStream *stream, uint periods, int deepSampleRate)
    : pktvisor::StreamMetricsHandler<DnsMetricsManager>(name, periods, deepSampleRate)
    , _stream(stream)
{
    assert(stream);
}

void DnsStreamHandler::start()
{
    if (_running) {
        return;
    }

    _pkt_udp_connection = _stream->udp_signal.connect(&DnsStreamHandler::process_udp_packet_cb, this);
    _start_tstamp_connection = _stream->start_tstamp_signal.connect(&DnsStreamHandler::set_initial_tstamp, this);

    _running = true;
}

void DnsStreamHandler::stop()
{
    if (!_running) {
        return;
    }

    _pkt_udp_connection.disconnect();
    _start_tstamp_connection.disconnect();

    _running = false;
}

DnsStreamHandler::~DnsStreamHandler()
{
}

// callback from input module
void DnsStreamHandler::process_udp_packet_cb(pcpp::Packet &payload, PacketDirection dir, pcpp::ProtocolType l3, uint32_t flowkey, timespec stamp)
{
    pcpp::UdpLayer *udpLayer = payload.getLayerOfType<pcpp::UdpLayer>();
    assert(udpLayer);

    uint16_t port{0};
    auto dst_port = ntohs(udpLayer->getUdpHeader()->portDst);
    auto src_port = ntohs(udpLayer->getUdpHeader()->portSrc);
    if (DnsLayer::isDnsPort(dst_port)) {
        port = dst_port;
    } else if (DnsLayer::isDnsPort(src_port)) {
        port = src_port;
    }
    if (port) {
        DnsLayer dnsLayer(udpLayer, &payload);
        _metrics->process_dns_layer(dnsLayer, dir, l3, pcpp::UDP, flowkey, port, stamp);
    }
}

void TcpSessionData::receive_dns_wire_data(const char *data, size_t len)
{
    const size_t MIN_DNS_QUERY_SIZE = 17;
    const size_t MAX_DNS_QUERY_SIZE = 512;

    _buffer.append(data, len);

    for (;;) {
        std::uint16_t size;

        if (_buffer.size() < sizeof(size))
            break;

        // dns packet size is in network byte order.
        size = static_cast<unsigned char>(_buffer[1]) | static_cast<unsigned char>(_buffer[0]) << 8;

        // ensure we never allocate more than max
        if (size < MIN_DNS_QUERY_SIZE || size > MAX_DNS_QUERY_SIZE) {
            break;
        }

        if (_buffer.size() >= sizeof(size) + size) {
            auto data = std::make_unique<char[]>(size);
            std::memcpy(data.get(), _buffer.data() + sizeof(size), size);
            _buffer.erase(0, sizeof(size) + size);
            _got_dns_msg(std::move(data), size);
        } else {
            // Nope, we need more data.
            break;
        }
    }
}

void DnsStreamHandler::tcp_message_ready_cb(int8_t side, const pcpp::TcpStreamData &tcpData)
{
    auto flowKey = tcpData.getConnectionData().flowKey;

    // check if this flow already appears in the connection manager. If not add it
    auto iter = _tcp_connections.find(flowKey);

    // if not tracking connection, and it's DNS, then start tracking.
    {
        uint16_t port{0};
        if (DnsLayer::isDnsPort(tcpData.getConnectionData().dstPort)) {
            port = tcpData.getConnectionData().dstPort;
        } else if (DnsLayer::isDnsPort(tcpData.getConnectionData().srcPort)) {
            port = tcpData.getConnectionData().srcPort;
        }
        if (iter == _tcp_connections.end() && port) {
            _tcp_connections.emplace(flowKey, TcpFlowData(tcpData.getConnectionData().srcIP.getType() == pcpp::IPAddress::IPv4AddressType, port));
            iter = _tcp_connections.find(tcpData.getConnectionData().flowKey);
        } else {
            // not tracking
            return;
        }
    }

    pcpp::ProtocolType l3Type{iter->second.l3Type};
    auto port{iter->second.port};
    timespec stamp{0, 0};
    // TODO IS THIS TIME WRONG, does it affect our window
    TIMEVAL_TO_TIMESPEC(&tcpData.getConnectionData().endTime, &stamp)
    auto dir = (side == 0) ? PacketDirection::fromHost : PacketDirection::toHost;

    auto got_dns_message = [this, port, dir, l3Type, flowKey, stamp](std::unique_ptr<const char[]> data, size_t size) {
        DnsLayer dnsLayer(const_cast<u_int8_t *>(reinterpret_cast<const uint8_t *>(data.get())), size, nullptr, nullptr);
        _metrics->process_dns_layer(dnsLayer, dir, l3Type, pcpp::TCP, flowKey, port, stamp);
    };
    if (!iter->second._sessionData[side].get()) {
        iter->second._sessionData[side] = std::make_shared<TcpSessionData>(got_dns_message);
    }
    iter->second._sessionData[side]->receive_dns_wire_data(reinterpret_cast<const char *>(tcpData.getData()), tcpData.getDataLength());
}

void DnsStreamHandler::tcp_connection_start_cb(const pcpp::ConnectionData &connectionData)
{
    // look for the connection
    auto iter = _tcp_connections.find(connectionData.flowKey);

    // start a new connection if it's DNS
    uint16_t port{0};
    if (DnsLayer::isDnsPort(connectionData.dstPort)) {
        port = connectionData.dstPort;
    } else if (DnsLayer::isDnsPort(connectionData.srcPort)) {
        port = connectionData.srcPort;
    }
    if (iter == _tcp_connections.end() && port) {
        // add it to the connections
        _tcp_connections.emplace(connectionData.flowKey, TcpFlowData(connectionData.srcIP.getType() == pcpp::IPAddress::IPv4AddressType, port));
    }
}

void DnsStreamHandler::tcp_connection_end_cb(const pcpp::ConnectionData &connectionData, pcpp::TcpReassembly::ConnectionEndReason reason)
{
    // find the connection in the connections by the flow key
    auto iter = _tcp_connections.find(connectionData.flowKey);

    // connection wasn't found, we didn't track
    if (iter == _tcp_connections.end()) {
        return;
    }

    // remove the connection from the connection manager
    _tcp_connections.erase(iter);
}

void DnsStreamHandler::toJSON(json &j, uint64_t period, bool merged)
{
    if (merged) {
        _metrics->toJSONMerged(j["dns"], period);
    } else {
        _metrics->toJSONSingle(j["dns"], period);
    }
}
void DnsStreamHandler::set_initial_tstamp(timespec stamp)
{
    _metrics->set_initial_tstamp(stamp);
}
json DnsStreamHandler::info_json() const
{
    json result;
    result["xact"]["open"] = _metrics->num_open_transactions();
    return result;
}

void DnsMetricsBucket::specialized_merge(const AbstractMetricsBucket &o)
{
    // static because caller guarantees only our own bucket type
    const auto &other = static_cast<const DnsMetricsBucket &>(o);

    std::shared_lock r_lock(other._mutex);
    std::unique_lock w_lock(_mutex);

    _DNS_xacts_total += other._DNS_xacts_total;
    _DNS_xacts_in += other._DNS_xacts_in;
    _DNS_xacts_out += other._DNS_xacts_out;
    _DNS_queries += other._DNS_queries;
    _DNS_replies += other._DNS_replies;
    _DNS_TCP += other._DNS_TCP;
    _DNS_IPv6 += other._DNS_IPv6;
    _DNS_NX += other._DNS_NX;
    _DNS_REFUSED += other._DNS_REFUSED;
    _DNS_SRVFAIL += other._DNS_SRVFAIL;
    _DNS_NOERROR += other._DNS_NOERROR;

    _dnsXactFromTimeUs.merge(other._dnsXactFromTimeUs);
    _dnsXactToTimeUs.merge(other._dnsXactToTimeUs);

    datasketches::cpc_union merge_qnameCard;
    merge_qnameCard.update(_dns_qnameCard);
    merge_qnameCard.update(other._dns_qnameCard);
    _dns_qnameCard = merge_qnameCard.get_result();

    _dns_topQname2.merge(other._dns_topQname2);
    _dns_topQname3.merge(other._dns_topQname3);
    _dns_topNX.merge(other._dns_topNX);
    _dns_topREFUSED.merge(other._dns_topREFUSED);
    _dns_topSRVFAIL.merge(other._dns_topSRVFAIL);
    _dns_topUDPPort.merge(other._dns_topUDPPort);
    _dns_topQType.merge(other._dns_topQType);
    _dns_topRCode.merge(other._dns_topRCode);
    _dns_slowXactIn.merge(other._dns_slowXactIn);
    _dns_slowXactOut.merge(other._dns_slowXactOut);
}

void DnsMetricsBucket::toJSON(json &j) const
{

    const double fractions[4]{0.50, 0.90, 0.95, 0.99};

    auto [num_events, num_samples] = event_data(); // thread safe
    std::shared_lock r_lock(_mutex);

    j["dns"]["wire_packets"]["total"] = num_events;
    j["dns"]["wire_packets"]["queries"] = _DNS_queries;
    j["dns"]["wire_packets"]["replies"] = _DNS_replies;
    j["dns"]["wire_packets"]["tcp"] = _DNS_TCP;
    j["dns"]["wire_packets"]["udp"] = num_events - _DNS_TCP;
    j["dns"]["wire_packets"]["ipv4"] = num_events - _DNS_IPv6;
    j["dns"]["wire_packets"]["ipv6"] = _DNS_IPv6;
    j["dns"]["wire_packets"]["nxdomain"] = _DNS_NX;
    j["dns"]["wire_packets"]["refused"] = _DNS_REFUSED;
    j["dns"]["wire_packets"]["srvfail"] = _DNS_SRVFAIL;
    j["dns"]["wire_packets"]["noerror"] = _DNS_NOERROR;

    j["dns"]["cardinality"]["qname"] = lround(_dns_qnameCard.get_estimate());
    j["dns"]["xact"]["counts"]["total"] = _DNS_xacts_total;

    {
        j["dns"]["xact"]["in"]["total"] = _DNS_xacts_in;
        j["dns"]["xact"]["in"]["top_slow"] = nlohmann::json::array();
        auto items = _dns_slowXactIn.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["xact"]["in"]["top_slow"][i]["name"] = items[i].get_item();
            j["dns"]["xact"]["in"]["top_slow"][i]["estimate"] = items[i].get_estimate();
        }
    }

    auto d_quantiles = _dnsXactFromTimeUs.get_quantiles(fractions, 4);
    if (d_quantiles.size()) {
        j["dns"]["xact"]["out"]["quantiles_us"]["p50"] = d_quantiles[0];
        j["dns"]["xact"]["out"]["quantiles_us"]["p90"] = d_quantiles[1];
        j["dns"]["xact"]["out"]["quantiles_us"]["p95"] = d_quantiles[2];
        j["dns"]["xact"]["out"]["quantiles_us"]["p99"] = d_quantiles[3];
    }

    d_quantiles = _dnsXactToTimeUs.get_quantiles(fractions, 4);
    if (d_quantiles.size()) {
        j["dns"]["xact"]["in"]["quantiles_us"]["p50"] = d_quantiles[0];
        j["dns"]["xact"]["in"]["quantiles_us"]["p90"] = d_quantiles[1];
        j["dns"]["xact"]["in"]["quantiles_us"]["p95"] = d_quantiles[2];
        j["dns"]["xact"]["in"]["quantiles_us"]["p99"] = d_quantiles[3];
    }

    {
        j["dns"]["xact"]["out"]["total"] = _DNS_xacts_out;
        j["dns"]["xact"]["out"]["top_slow"] = nlohmann::json::array();
        auto items = _dns_slowXactOut.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["xact"]["out"]["top_slow"][i]["name"] = items[i].get_item();
            j["dns"]["xact"]["out"]["top_slow"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_qname2"] = nlohmann::json::array();
        auto items = _dns_topQname2.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["top_qname2"][i]["name"] = items[i].get_item();
            j["dns"]["top_qname2"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_qname3"] = nlohmann::json::array();
        auto items = _dns_topQname3.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["top_qname3"][i]["name"] = items[i].get_item();
            j["dns"]["top_qname3"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_nxdomain"] = nlohmann::json::array();
        auto items = _dns_topNX.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["top_nxdomain"][i]["name"] = items[i].get_item();
            j["dns"]["top_nxdomain"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_refused"] = nlohmann::json::array();
        auto items = _dns_topREFUSED.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["top_refused"][i]["name"] = items[i].get_item();
            j["dns"]["top_refused"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_srvfail"] = nlohmann::json::array();
        auto items = _dns_topSRVFAIL.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            j["dns"]["top_srvfail"][i]["name"] = items[i].get_item();
            j["dns"]["top_srvfail"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_rcode"] = nlohmann::json::array();
        auto items = _dns_topRCode.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            if (RCodeNames.find(items[i].get_item()) != RCodeNames.end()) {
                j["dns"]["top_rcode"][i]["name"] = RCodeNames[items[i].get_item()];
            } else {
                std::stringstream keyBuf;
                keyBuf << items[i].get_item();
                j["dns"]["top_rcode"][i]["name"] = keyBuf.str();
            }
            j["dns"]["top_rcode"][i]["estimate"] = items[i].get_estimate();
        }
    }

    {
        j["dns"]["top_qtype"] = nlohmann::json::array();
        auto items = _dns_topQType.get_frequent_items(datasketches::frequent_items_error_type::NO_FALSE_NEGATIVES);
        for (uint64_t i = 0; i < std::min(10UL, items.size()); i++) {
            if (QTypeNames.find(items[i].get_item()) != QTypeNames.end()) {
                j["dns"]["top_qtype"][i]["name"] = QTypeNames[items[i].get_item()];
            } else {
                std::stringstream keyBuf;
                keyBuf << items[i].get_item();
                j["dns"]["top_qtype"][i]["name"] = keyBuf.str();
            }
            j["dns"]["top_qtype"][i]["estimate"] = items[i].get_estimate();
        }
    }
}

// the main bucket analysis
void DnsMetricsBucket::process_dns_layer(bool deep, DnsLayer &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, uint32_t flowkey, uint16_t port, timespec stamp)
{

    std::unique_lock lock(_mutex);

    if (l3 == pcpp::IPv6) {
        _DNS_IPv6++;
    }

    if (l4 == pcpp::TCP) {
        _DNS_TCP++;
    }

    // only count response codes on responses (not queries)
    if (payload.getDnsHeader()->queryOrResponse == QR::response) {
        _DNS_replies++;
        switch (payload.getDnsHeader()->responseCode) {
        case NoError:
            _DNS_NOERROR++;
            break;
        case SrvFail:
            _DNS_SRVFAIL++;
            break;
        case NXDomain:
            _DNS_NX++;
            break;
        case Refused:
            _DNS_REFUSED++;
            break;
        }
    } else {
        _DNS_queries++;
    }

    if (!deep) {
        return;
    }

    payload.parseResources(true);

    if (payload.getDnsHeader()->queryOrResponse == response) {
        _dns_topRCode.update(payload.getDnsHeader()->responseCode);
    }

    auto query = payload.getFirstQuery();
    if (query) {

        auto name = query->getName();

        _dns_qnameCard.update(name);
        _dns_topQType.update(query->getDnsType());

        if (payload.getDnsHeader()->queryOrResponse == response) {
            switch (payload.getDnsHeader()->responseCode) {
            case SrvFail:
                _dns_topSRVFAIL.update(name);
                break;
            case NXDomain:
                _dns_topNX.update(name);
                break;
            case Refused:
                _dns_topREFUSED.update(name);
                break;
            }
        }

        auto aggDomain = aggregateDomain(name);
        _dns_topQname2.update(std::string(aggDomain.first));
        if (aggDomain.second.size()) {
            _dns_topQname3.update(std::string(aggDomain.second));
        }
    }

    _dns_topUDPPort.update(port);
}

void DnsMetricsBucket::newDNSXact(bool deep, float to90th, float from90th, DnsLayer &dns, PacketDirection dir, DnsTransaction xact)
{

    uint64_t xactTime = ((xact.totalTS.tv_sec * 1'000'000'000L) + xact.totalTS.tv_nsec) / 1'000; // nanoseconds to microseconds

    // lock for write
    std::unique_lock lock(_mutex);

    _DNS_xacts_total++;

    if (dir == PacketDirection::toHost) {
        _DNS_xacts_out++;
        if (deep) {
            _dnsXactFromTimeUs.update(xactTime);
        }
    } else if (dir == PacketDirection::fromHost) {
        _DNS_xacts_in++;
        if (deep) {
            _dnsXactToTimeUs.update(xactTime);
        }
    }

    if (deep) {
        auto query = dns.getFirstQuery();
        if (query) {
            auto name = query->getName();
            // dir is the direction of the last packet, meaning the reply so from a transaction perspective
            // we look at it from the direction of the query, so the opposite side than we have here
            if (dir == PacketDirection::toHost && from90th > 0.0 && xactTime >= from90th) {
                _dns_slowXactOut.update(name);
            } else if (dir == PacketDirection::fromHost && to90th > 0.0 && xactTime >= to90th) {
                _dns_slowXactIn.update(name);
            }
        }
    }
}

// the general metrics manager entry point (both UDP and TCP)
void DnsMetricsManager::process_dns_layer(DnsLayer &payload, PacketDirection dir, pcpp::ProtocolType l3, pcpp::ProtocolType l4, uint32_t flowkey, uint16_t port, timespec stamp)
{
    // base event
    new_event(stamp);
    // handle dns transactions (query/response pairs)
    if (payload.getDnsHeader()->queryOrResponse == QR::response) {
        auto xact = _qr_pair_manager.maybeEndDnsTransaction(flowkey, payload.getDnsHeader()->transactionID, stamp);
        if (xact.first) {
            _metricBuckets.back()->newDNSXact(_shouldDeepSample, _to90th, _from90th, payload, dir, xact.second);
        }
    } else {
        _qr_pair_manager.startDnsTransaction(flowkey, payload.getDnsHeader()->transactionID, stamp);
    }
    // process in the "live" bucket
    _metricBuckets.back()->process_dns_layer(_shouldDeepSample, payload, dir, l3, l4, flowkey, port, stamp);
}

}