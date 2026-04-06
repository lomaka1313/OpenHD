#include "ethernet_link.h"

#include "../lib/wifibroadcast/wifibroadcast/src/fec/FEC.h"
#include "BlockSizeHelper.hpp"
#include "openhd_util.h"

EthernetTx::EthernetTx(std::string host, int port, bool qos) {
    m_tx = std::make_unique<openhd::UDPForwarder>(host, port, qos);
    m_fec_encoder = std::make_unique<FECEncoder>();
    auto cb = [this](const uint8_t *packet, int packet_len) {
        m_tx->forwardPacketViaUDP(packet, packet_len);
        m_n_injected_packets++;
        m_count_bytes_data_injected += packet_len;
    };
    m_fec_encoder->m_out_cb = cb;
    openhd::log::get_default()->debug("Started EthernetTx via UDP for host {} on port {}", host, port);
}

EthernetTx::~EthernetTx() {
}

void EthernetTx::transmit_telemetry_data(const OHDLink::TelemetryTxPacket &packet) {
    m_n_input_packets++;
    m_count_bytes_data_provided += packet.data->size();
    m_fec_encoder->encode_block({packet.data}, 0);
}

void EthernetTx::transmit_video_data(const openhd::FragmentedVideoFrame &fragmented_video_frame) {
    m_n_input_packets++;
    if (fragmented_video_frame.dirty_frame != nullptr) {
        // non rtp
        const int MTU = 1400;
        const int n_primary_fragments = blocksize::div_ceil(fragmented_video_frame.dirty_frame->size(), MTU);
        const int n_secondary_fragments = calculate_n_secondary_fragments(n_primary_fragments, m_video_fec_percentage);
        m_count_bytes_data_provided += fragmented_video_frame.dirty_frame->size();
        m_fec_encoder->fragment_and_encode(fragmented_video_frame.dirty_frame->data(),
                                           fragmented_video_frame.dirty_frame->size(),
                                           n_primary_fragments,
                                           n_secondary_fragments);
    } else {
        auto blocks = blocksize::split_frame_if_needed(fragmented_video_frame.rtp_fragments, m_video_fec_percentage);
        for (auto &x_block: blocks) {
            m_count_bytes_data_provided += x_block.size();
            const auto n_secondary_f = calculate_n_secondary_fragments(x_block.size(), m_video_fec_percentage);
            m_fec_encoder->encode_block(x_block, n_secondary_f);
        }
    }
}

EthernetTx::Statistics EthernetTx::get_latest_stats() {
    Statistics ret{};
    ret.n_provided_bytes = m_count_bytes_data_provided;
    ret.n_provided_packets = m_n_input_packets;
    ret.n_injected_packets = m_n_injected_packets;
    ret.n_injected_bytes = static_cast<int64_t>(m_count_bytes_data_injected);
    ret.current_injected_bits_per_second =
        m_bitrate_calculator_injected_bytes.get_last_or_recalculate(
            m_count_bytes_data_injected, std::chrono::seconds(2));
    ret.current_provided_bits_per_second =
        m_bitrate_calculator_data_provided.get_last_or_recalculate(
            m_count_bytes_data_provided, std::chrono::seconds(2));
    ret.n_dropped_packets = m_n_dropped_packets;
    ret.n_dropped_frames = m_n_dropped_frames;
    ret.current_injected_packets_per_second =
        m_packets_per_second_calculator.get_last_or_recalculate(
            m_n_injected_packets, std::chrono::seconds(2));
    ret.curr_block_until_tx_min_us = m_curr_block_until_tx_min_max_avg_us.min;
    ret.curr_block_until_tx_max_us = m_curr_block_until_tx_min_max_avg_us.max;
    ret.curr_block_until_tx_avg_us = m_curr_block_until_tx_min_max_avg_us.avg;
    return ret;
}

EthernetTx::FECStats EthernetTx::get_latest_fec_stats() const {
    FECStats ret{};
    if (m_fec_encoder) {
        ret.curr_fec_encode_time = m_fec_encoder->m_curr_fec_block_encode_time;
        ret.curr_fec_block_length = m_fec_encoder->m_curr_fec_block_sizes;
    }
    return ret;
}
