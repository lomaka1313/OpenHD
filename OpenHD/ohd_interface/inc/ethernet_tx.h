#pragma once

#include <memory>

#include "../lib/wifibroadcast/wifibroadcast/src/fec/FECEncoder.h"
#include "openhd_udp.h"

class EthernetTx {
public:
    EthernetTx(std::string host, int port, bool qos);

    ~EthernetTx();


    struct Statistics {
        int64_t n_provided_packets;
        int64_t n_provided_bytes;
        int64_t n_injected_packets;
        int64_t n_injected_bytes;
        uint64_t current_provided_bits_per_second;
        uint64_t current_injected_bits_per_second;
        // Other than bits per second, packets per second is also an important
        // metric - Sending a lot of small packets for example should be avoided
        uint64_t current_injected_packets_per_second;
        // N of dropped packets, increases when both the internal driver queue and
        // the extra 124 packets queue of the tx fill up In FEC mode (video), every
        // time a frame is dropped this is increased by the n of fragments in this
        // frame
        uint64_t n_dropped_packets;
        int32_t n_dropped_frames;
        // only for frame (FEC) mode
        uint32_t curr_block_until_tx_min_us;
        uint32_t curr_block_until_tx_max_us;
        uint32_t curr_block_until_tx_avg_us;
    };

    void transmit_video_data(const openhd::FragmentedVideoFrame &fragmented_video_frame);

    void transmit_telemetry_data(const OHDLink::TelemetryTxPacket &packet);

    Statistics get_latest_stats();

    struct FECStats {
        MinMaxAvg<std::chrono::nanoseconds> curr_fec_encode_time{};
        MinMaxAvg<uint16_t> curr_fec_block_length{};
    };

    FECStats get_latest_fec_stats() const;

private:
    std::unique_ptr<openhd::UDPForwarder> m_tx;
    std::unique_ptr<FECEncoder> m_fec_encoder;

    int m_video_fec_percentage = 30;

    uint64_t m_count_bytes_data_provided = 0;
    int64_t m_n_input_packets = 0;
    int64_t m_n_injected_packets = 0;
    uint64_t m_count_bytes_data_injected = 0;
    BitrateCalculator m_bitrate_calculator_injected_bytes{};
    BitrateCalculator m_bitrate_calculator_data_provided{};
    uint64_t m_n_dropped_packets = 0;
    int32_t m_n_dropped_frames = 0;
    PacketsPerSecondCalculator m_packets_per_second_calculator{};
    MinMaxAvg<uint32_t> m_curr_block_until_tx_min_max_avg_us{0, 0, 0};
};
