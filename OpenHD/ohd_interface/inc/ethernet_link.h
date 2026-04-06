#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "openhd_config.h"
#include "openhd_link.hpp"
#include "openhd_udp.h"

#include "ethernet_link_settings.h"
#include "ethernet_tx.h"
#include "ethernet_rx.h"

namespace openhd::link_statistics { struct StatsAirGround; }


class EthernetLink : public ConfigurableOHDLink {
public:
  static std::shared_ptr<EthernetLink> create(OHDProfile profile);

  // ConfigurableOHDLink implementations
  std::vector<openhd::Setting> get_all_settings() override;

  // OHDLink implementations
  void transmit_telemetry_data(TelemetryTxPacket packet) override;

protected:
  EthernetLink(OHDProfile profile);
  virtual bool is_air() const = 0;
  virtual void update_statistics(openhd::link_statistics::StatsAirGround& stats) {}
  virtual void on_settings_update();

protected:
  const EthernetLinkSettings& settings() const;
  void start_work_thread();
  void stop_work_thread();

private:
  void create_telemetry_tx();
  void create_telemetry_rx();
  void update_statistics();
  void loop_do_work();

private:
  std::unique_ptr<EthernetLinkSettingsHolder> m_settings;

  bool m_work_thread_run = false;
  std::thread m_work_thread;

  std::unique_ptr<EthernetTx> m_telemetry_tx;
  std::unique_ptr<EthernetRx> m_telemetry_rx;
};


class EthernetLinkAir final : public EthernetLink {
public:
  EthernetLinkAir(OHDProfile profile);
  ~EthernetLinkAir();

  // OHDLink implementations
  void transmit_video_data(
      int stream_index,
      const openhd::FragmentedVideoFrame& fragmented_video_frame) override;
  void transmit_audio_data(const openhd::AudioPacket& audio_packet) override;

private:
  bool is_air() const override { return true; }
  void update_statistics(openhd::link_statistics::StatsAirGround& stats) override;
  void on_settings_update() override;

  void create_video_tx();

private:
  std::unique_ptr<EthernetTx> m_video_tx;

  std::atomic_int m_total_dropped_frames = 0;
};


class EthernetLinkGround final : public EthernetLink {
public:
  EthernetLinkGround(OHDProfile profile);
  ~EthernetLinkGround();

  // OHDLink implementations
  void transmit_video_data(
      int stream_index,
      const openhd::FragmentedVideoFrame& fragmented_video_frame) override;
  void transmit_audio_data(const openhd::AudioPacket& audio_packet) override;

private:
  bool is_air() const override { return false; }
  void update_statistics(openhd::link_statistics::StatsAirGround& stats) override;
  void on_settings_update() override;

  void create_video_rx();

private:
  std::unique_ptr<EthernetRx> m_video_rx;
};
