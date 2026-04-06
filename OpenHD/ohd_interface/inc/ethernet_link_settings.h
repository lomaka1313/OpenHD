#pragma once

#include "openhd_settings_persistent.h"

#include "openhd_profile.h"

// Ethernet link settings (connection via Starlink, LTE etc).
struct EthernetLinkSettings {
  // Enables/disables ethernet link. When Ethernet link is enabled, wifibroadcast link is not used.
  bool enabled = false;
  // For Gnd unit: the IP of the Air unit to connect to (for both, video and telemetry). Default: "10.0.0.2".
  // For Air unit: the IP of the Gnd unit to connect to (for both, video and telemetry). Default: "10.0.0.3".
  std::string remote_ip;  // default value is initialized during creation depending on is_air.
  int video_port = 5000;
  int telemetry_port = 5001;
};

class EthernetLinkSettingsHolder : public openhd::PersistentSettings<EthernetLinkSettings> {
 public:
  EthernetLinkSettingsHolder(const OHDProfile &profile);

 private:
  [[nodiscard]] std::string get_unique_filename() const override { return "ethernet_link_settings.json"; }
  [[nodiscard]] EthernetLinkSettings create_default() const override;
  std::optional<EthernetLinkSettings> impl_deserialize(const std::string& file_as_string) const override;
  std::string imp_serialize(const EthernetLinkSettings& data) const override;

 private:
  const OHDProfile m_profile;
};
