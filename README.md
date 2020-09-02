pktvisor 
============
> This project is in [active development](https://github.com/ns1/community/blob/master/project_status/ACTIVE_DEVELOPMENT.md).

pktvisor summarizes data streams (e.g. via packet capture) in real time and provides a clean, time-windowed HTTP interface and command line UI to the results.  

2019-2020© NSONE, Inc.

Overview
--------

pktvisor consists of:
1. A collector agent
1. A terminal based, command line UI which can visualize summarized data from the agent
1. Tools for collecting and visualizing a globally distributed set of agents to a central location


Getting Started
---------------

The easiest way to get started with pktvisor is to use the public docker image. The image contains both the command line UI and the collector daemon (agent).
```
docker pull ns1labs/pktvisor 
# command line UI
docker run ns1labs/pktvisor pktvisor --help
# collector daemon/agent
docker run ns1labs/pktvisor pktvisord --help
```

See usage examples below.

There are currently no prebuilt operating system packages. If you would like to build your own executable,
please see the Build section below.

Agent Usage
-----------

A collector daemon agent should be installed on each note to be monitored.

Current command line options are described with:

```
pktvisord --help
```

```

    Usage:
      pktvisord [-b BPF] [-l HOST] [-p PORT] [-H HOSTSPEC] [--periods P] [--summary] [--geo-city FILE] [--geo-asn FILE]
                [--max-deep-sample N]
                TARGET
      pktvisord (-h | --help)
      pktvisord --version

    pktvisord summarizes your data streams.

    TARGET is either a network interface, an IP address (4 or 6) or a pcap file (ending in .pcap or .cap)

    Options:
      -l HOST               Run metrics webserver on the given host or IP [default: localhost]
      -p PORT               Run metrics webserver on the given port [default: 10853]
      -b BPF                Filter packets using the given BPF string
      --geo-city FILE       GeoLite2 City database to use for IP to Geo mapping (if enabled)
      --geo-asn FILE        GeoLite2 ASN database to use for IP to ASN mapping (if enabled)
      --max-deep-sample N   Never deep sample more than N% of packets (an int between 0 and 100) [default: 100]
      --periods P           Hold this many 60 second time periods of history in memory [default: 5]
      --summary             Instead of a time window with P periods, summarize all packets into one bucket for entire time period.
                            Useful for executive summary of (and applicable only to) a pcap file. [default: false]
      -H HOSTSPEC           Specify subnets (comma separated) to consider HOST, in CIDR form. In live capture this /may/ be detected automatically
                            from capture device but /must/ be specified for pcaps. Example: "10.0.1.0/24,10.0.2.1/32,2001:db8::/64"
                            Specifying this for live capture will append to any automatic detection.
      -h --help             Show this screen
      --version             Show version

```

Command Line UI Usage
=====================

The command line UI connects to an agent to visualize real time stream summarization. It can connect to a local or remote agent.

Usage Examples
==============

Starting the collector agent from Docker with GeoDB and Host options:

```
docker run --rm --net=host -d --mount type=bind,source=/opt/geo,target=/geo ns1labs/pktvisor pktvisord --geo-city /geo/GeoIP2-City.mmdb --geo-asn /geo/GeoIP2-ISP.mmdb -H 192.168.0.54/32,127.0.0.1/32 any
```

Running the console UI from Docker:
```
docker run -it --rm --net=host ns1labs/pktvisor pktvisor
```

Centralized Collection
======================

pktvisor may be collected centrally to give a global view of the collected information.