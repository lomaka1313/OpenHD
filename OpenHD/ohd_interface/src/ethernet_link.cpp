#include "ethernet_link.h"

#include "../lib/wifibroadcast/wifibroadcast/src/fec/FEC.h"
#include "BlockSizeHelper.hpp"
#include "openhd_config.h"
#include "openhd_util.h"
#include "wb_link.h"


std::shared_ptr<EthernetLink> EthernetLink::create(OHDProfile profile) {
    if (profile.is_air) {
        return std::make_shared<EthernetLinkAir>(profile);
    } else {
        return std::make_shared<EthernetLinkGround>(profile);
    }
}

EthernetLink::EthernetLink(OHDProfile profile)
    : // Initialize telemetry transmitter and receiver for bidirectional telemetry
      m_settings {std::make_unique<EthernetLinkSettingsHolder>(profile)} {
    m_settings->register_listener([this]() { on_settings_update(); });
    create_telemetry_tx();
    create_telemetry_rx();
}

void EthernetLink::create_telemetry_tx() {
    m_telemetry_tx = std::make_unique<EthernetTx>(settings().remote_ip, settings().telemetry_port, true);
}

void EthernetLink::create_telemetry_rx() {
    m_telemetry_rx = std::make_unique<EthernetRx>(settings().telemetry_port,
        [this](const uint8_t *data, std::size_t len) {
            auto shared = std::make_shared<std::vector<uint8_t> >(data, data + len);
            on_receive_telemetry_data(shared);
        }
    );
}

void EthernetLink::on_settings_update() {
  create_telemetry_tx();
  create_telemetry_rx();
}

std::vector<openhd::Setting> EthernetLink::get_all_settings() {
  std::vector<openhd::Setting> ret;
  // NOTE: Parameter names must be no longer then 16 chars.
  ret.push_back(openhd::Setting{
      "ELINK_ENABLED",
      openhd::IntSetting{
          settings().enabled,
          [this](std::string, int value) {
              m_settings->unsafe_get_settings().enabled = value;
              m_settings->persist();
              return true;
          }
      }
  });
  ret.push_back(openhd::Setting{
      "ELINK_REMOTE_IP",
      openhd::StringSetting{
          settings().remote_ip,
          [this](std::string, std::string value) {
              if (value.empty()) {
                return false;
              }
              m_settings->unsafe_get_settings().remote_ip = value;
              m_settings->persist();
              return true;
          }
      }
  });
  ret.push_back(openhd::Setting{
      "ELINK_VIDEO_PORT",
      openhd::IntSetting{
          settings().video_port,
          [this](std::string, int value) {
              if (value <= 0) {
                return false;
              }
              m_settings->unsafe_get_settings().video_port = value;
              m_settings->persist();
              return true;
          }
      }
  });
  ret.push_back(openhd::Setting{
      "ELINK_TELEM_PORT",
      openhd::IntSetting{
          settings().telemetry_port,
          [this](std::string, int value) {
              if (value <= 0) {
                return false;
              }
              m_settings->unsafe_get_settings().telemetry_port = value;
              m_settings->persist();
              return true;
          }
      }
  });
  return ret;
}

const EthernetLinkSettings& EthernetLink::settings() const {
    return m_settings->get_settings();
}

void EthernetLink::start_work_thread() {
  m_work_thread_run = true;
  m_work_thread = std::thread(&EthernetLink::loop_do_work, this);
}

void EthernetLink::stop_work_thread() {
    m_work_thread_run = false;
    if (m_work_thread.joinable()) {
        m_work_thread.join();
    }
}

void EthernetLink::transmit_telemetry_data(TelemetryTxPacket packet) {
    // Send telemetry data to the destination
    m_telemetry_tx->transmit_telemetry_data(packet);
}

void EthernetLink::loop_do_work() {
    while (m_work_thread_run) {
        update_statistics();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void EthernetLink::update_statistics() {
    // telemetry is available on both air and ground
    openhd::link_statistics::StatsAirGround stats{};
    const auto curr_tx_stats = m_telemetry_tx->get_latest_stats();
    const auto curr_rx_stats = m_telemetry_rx->get_latest_stats();
    stats.monitor_mode_link.curr_tx_bps = stats.telemetry.curr_tx_bps =
            curr_tx_stats.current_provided_bits_per_second;
    stats.telemetry.curr_tx_pps = stats.telemetry.curr_tx_pps =
            curr_tx_stats.current_injected_packets_per_second;
    stats.monitor_mode_link.curr_rx_bps = stats.telemetry.curr_rx_bps = curr_rx_stats.curr_in_bits_per_second;
    stats.monitor_mode_link.curr_rx_pps = stats.telemetry.curr_rx_pps = curr_rx_stats.curr_in_packets_per_second;
    update_statistics(stats);

    stats.is_air = is_air();
    stats.ready = true;
    openhd::LinkActionHandler::instance().update_link_stats(stats);
    // m_console->debug("Last received packet mcs:{}
    // chan_width:{}",rxStats.last_received_packet_mcs_index,rxStats.last_received_packet_channel_width);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EthernetLinkAir

EthernetLinkAir::EthernetLinkAir(OHDProfile profile)
    : EthernetLink(profile) {
    create_video_tx();
    start_work_thread(); // need to start thread, when object is fully constructed.
}

void EthernetLinkAir::create_video_tx() {
    // Initialize video transmitter for sending video to the ground unit
    m_video_tx = std::make_unique<EthernetTx>(settings().remote_ip, settings().video_port, false);
}

EthernetLinkAir::~EthernetLinkAir() {
    stop_work_thread();  // need to stop it explicitly before all the EthernetTx/EthernetRx are destructed.
}

void EthernetLinkAir::transmit_video_data(int stream_index,
                                          const openhd::FragmentedVideoFrame &fragmented_video_frame) {
    // Send video data fragments to the destination
    m_video_tx->transmit_video_data(fragmented_video_frame);
}

void EthernetLinkAir::transmit_audio_data(const openhd::AudioPacket &audio_packet) {
    // Currently not implemented for EthernetLink
}

void EthernetLinkAir::update_statistics(openhd::link_statistics::StatsAirGround& stats) {
    stats.monitor_mode_link.bitfield = openhd::link_statistics::write_monitor_link_bitfield({
        openhd::link_statistics::MonitorModeLinkBitfield{false, false,false, true}});
    stats.monitor_mode_link.curr_rx_packet_loss_perc = 0; // todo

    const auto curr_tx_stats = m_video_tx->get_latest_stats();
    // optimization - only send for active video links
    openhd::link_statistics::Xmavlink_openhd_stats_wb_video_air_t air_video{};
    openhd::link_statistics::
            Xmavlink_openhd_stats_wb_video_air_fec_performance_t air_fec{};
    int rec_bitrate = 0;
    auto cam_stats = openhd::LinkActionHandler::instance().get_cam_info(0);
    rec_bitrate = cam_stats.encoding_bitrate_kbits;
    air_video.curr_recommended_bitrate = rec_bitrate;
    air_video.curr_measured_encoder_bitrate = curr_tx_stats.current_provided_bits_per_second;
    air_video.curr_injected_bitrate = curr_tx_stats.current_injected_bits_per_second;
    air_video.curr_injected_pps = curr_tx_stats.current_injected_packets_per_second;
    const int tx_dropped_frames = m_total_dropped_frames.load();
    // const int tx_dropped_frames = curr_tx_stats.n_dropped_frames;
    air_video.curr_dropped_frames = tx_dropped_frames;
    const auto curr_tx_fec_stats = m_video_tx->get_latest_fec_stats();
    air_fec.curr_fec_encode_time_avg_us = openhd::util::get_micros(curr_tx_fec_stats.curr_fec_encode_time.avg);
    air_fec.curr_fec_encode_time_min_us = openhd::util::get_micros(curr_tx_fec_stats.curr_fec_encode_time.min);
    air_fec.curr_fec_encode_time_max_us = openhd::util::get_micros(curr_tx_fec_stats.curr_fec_encode_time.max);
    air_fec.curr_fec_block_size_min = curr_tx_fec_stats.curr_fec_block_length.min;
    air_fec.curr_fec_block_size_max = curr_tx_fec_stats.curr_fec_block_length.max;
    air_fec.curr_fec_block_size_avg = curr_tx_fec_stats.curr_fec_block_length.avg;
    air_fec.curr_tx_delay_min_us = curr_tx_stats.curr_block_until_tx_min_us;
    air_fec.curr_tx_delay_max_us = curr_tx_stats.curr_block_until_tx_max_us;
    air_fec.curr_tx_delay_avg_us = curr_tx_stats.curr_block_until_tx_avg_us;
    stats.air_fec_performance = air_fec;
    stats.stats_wb_video_air.push_back(air_video);
}

void EthernetLinkAir::on_settings_update() {
    EthernetLink::on_settings_update();
    create_video_tx();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EthernetLinkGround

EthernetLinkGround::EthernetLinkGround(OHDProfile profile)
    : EthernetLink(profile) {
    create_video_rx();
    start_work_thread(); // need to start thread, when object is fully constructed.
}

void EthernetLinkGround::create_video_rx() {
    // Initialize video receiver for receiving video from the air unit
    m_video_rx = std::make_unique<EthernetRx>(settings().video_port,
        [this](const uint8_t *data, std::size_t len) {
            on_receive_video_data(0, data, len); // Process incoming video
        }
    );
}

EthernetLinkGround::~EthernetLinkGround() {
    stop_work_thread();  // need to stop it explicitly before all the EthernetTx/EthernetRx are destructed.
}

void EthernetLinkGround::transmit_video_data(int stream_index,
                                             const openhd::FragmentedVideoFrame &fragmented_video_frame) {
    // Not expected to be called, as ground does not transmit video.
}

void EthernetLinkGround::transmit_audio_data(const openhd::AudioPacket &audio_packet) {
    // Not expected to be called, as ground does not transmit audio.
}

void EthernetLinkGround::update_statistics(openhd::link_statistics::StatsAirGround& stats) {
    const auto rx_stats = m_video_rx->get_latest_stats();
    openhd::link_statistics::Xmavlink_openhd_stats_wb_video_ground_t
            ground_video{};
    openhd::link_statistics::
            Xmavlink_openhd_stats_wb_video_ground_fec_performance_t gnd_fec{};
    ground_video.curr_incoming_bitrate = rx_stats.curr_out_bits_per_second;
    const auto fec_stats = m_video_rx->get_latest_fec_stats();
    ground_video.count_fragments_recovered = fec_stats.count_fragments_recovered;
    ground_video.count_blocks_recovered = fec_stats.count_blocks_recovered;
    ground_video.count_blocks_lost = fec_stats.count_blocks_lost;
    ground_video.count_blocks_total = fec_stats.count_blocks_total;
    gnd_fec.curr_fec_decode_time_avg_us = openhd::util::get_micros(fec_stats.curr_fec_decode_time.avg);
    gnd_fec.curr_fec_decode_time_min_us = openhd::util::get_micros(fec_stats.curr_fec_decode_time.min);
    gnd_fec.curr_fec_decode_time_max_us = openhd::util::get_micros(fec_stats.curr_fec_decode_time.max);
    stats.gnd_fec_performance = gnd_fec;
    stats.stats_wb_video_ground.push_back(ground_video);
}

void EthernetLinkGround::on_settings_update() {
    EthernetLink::on_settings_update();
    create_video_rx();
}
