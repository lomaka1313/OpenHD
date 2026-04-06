#include "ethernet_link.h"

#include "openhd_spdlog.h"

#include "wb_link.h"

EthernetRx::EthernetRx(int port, openhd::UDPReceiver::OUTPUT_DATA_CALLBACK on_received_cb) {
    m_rx = std::make_unique<openhd::UDPReceiver>(
        "0.0.0.0", port, [this](const uint8_t *data, std::size_t len) {
            handle_data(data, len); // Process incoming UDP data
        });
    m_fec_decoder = std::make_unique<FECDecoder>(1, 256, false, false);
    m_fec_decoder->mSendDecodedPayloadCallback = [this, on_received_cb](const uint8_t *data, int data_len) {
        m_n_output_bytes += data_len;
        on_received_cb(data, data_len);
    };
    // Start UDP receiver in the background
    m_rx->runInBackground();
    openhd::log::get_default()->debug("Started EthernetRx via UDP on port {}", port);
}

EthernetRx::~EthernetRx() {
}

void EthernetRx::handle_data(const uint8_t *data, int data_len) {
    m_n_input_packets++;
    m_n_input_bytes += data_len;
    const auto now = std::chrono::steady_clock::now();
    const auto diff = now - m_last_packet_received;
    // FEC decoder always try to restore lost data
    // we need to reset FEC queue if connection was lost
    if (diff >= std::chrono::seconds(3)) {
        m_fec_decoder->reset_rx_queue();
    }
    m_last_packet_received = now;
    m_fec_decoder->process_valid_packet(data, data_len);
}

EthernetRx::Statistics EthernetRx::get_latest_stats() {
    Statistics ret;
    ret.n_input_bytes = m_n_input_bytes;
    ret.n_input_packets = m_n_input_packets;
    ret.curr_in_packets_per_second =
        m_input_packets_per_second_calculator.get_last_or_recalculate(
            m_n_input_packets, std::chrono::seconds(2));
    ret.curr_in_bits_per_second =
        m_input_bitrate_calculator.get_last_or_recalculate(
            m_n_input_bytes, std::chrono::seconds(2));
    ret.curr_out_bits_per_second =
        m_received_bitrate_calculator.get_last_or_recalculate(
            m_n_output_bytes, std::chrono::seconds(2));
    ret.last_packet_received = m_last_packet_received.time_since_epoch().count();
    return ret;
}

EthernetRx::FECRxStats EthernetRx::get_latest_fec_stats() const {
    FECRxStats ret{};
    if (m_fec_decoder) {
        auto stats = m_fec_decoder->stats;
        ret.count_blocks_lost = stats.count_blocks_lost;
        ret.count_blocks_recovered = stats.count_blocks_recovered;
        ret.count_blocks_total = stats.count_blocks_total;
        ret.count_fragments_recovered = stats.count_fragments_recovered;
        ret.curr_fec_decode_time = stats.curr_fec_decode_time;
    }
    return ret;
}
