#pragma once

#include <memory>

#include "../lib/wifibroadcast/wifibroadcast/src/fec/FECDecoder.h"

class EthernetRx {
public:
    EthernetRx(int port, openhd::UDPReceiver::OUTPUT_DATA_CALLBACK on_received_cb);

    ~EthernetRx();

    void handle_data(const uint8_t *data, int data_len);

    struct Statistics {
        int64_t n_input_packets = 0;
        int64_t n_input_bytes = 0;
        int curr_in_packets_per_second = 0;
        int curr_in_bits_per_second = 0;
        int curr_out_bits_per_second = 0;
        int last_packet_received = 0;
    };

    Statistics get_latest_stats();

    struct FECRxStats {
        // total block count
        uint64_t count_blocks_total = 0;
        // a block counts as "lost" if it was removed before being fully received or
        // recovered
        uint64_t count_blocks_lost = 0;
        // a block counts as "recovered" if it was recovered using FEC packets
        uint64_t count_blocks_recovered = 0;
        // n of primary fragments that were reconstructed during the recovery
        // process of a block
        uint64_t count_fragments_recovered = 0;
        // n of forwarded bytes
        uint64_t count_bytes_forwarded = 0;
        MinMaxAvg<std::chrono::nanoseconds> curr_fec_decode_time{};
    };

    FECRxStats get_latest_fec_stats() const;

private:
    int64_t m_n_input_bytes = 0;
    int64_t m_n_input_packets = 0;
    std::chrono::steady_clock::time_point m_last_packet_received;
    std::atomic<int64_t> m_n_output_bytes = 0;

    BitrateCalculator m_received_bitrate_calculator{};
    BitrateCalculator m_input_bitrate_calculator{};
    PacketsPerSecondCalculator m_input_packets_per_second_calculator{};
    std::unique_ptr<FECDecoder> m_fec_decoder;
    std::unique_ptr<openhd::UDPReceiver> m_rx;
};
