#include "ethernet_link_settings.h"

#include "openhd_settings_directories.h"

#include "include_json.hpp"


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EthernetLinkSettings,
    enabled, remote_ip, video_port, telemetry_port);

EthernetLinkSettingsHolder::EthernetLinkSettingsHolder(const OHDProfile &profile)
    : openhd::PersistentSettings<EthernetLinkSettings>(openhd::get_interface_settings_directory()),
      m_profile(profile) {
  init();
}

EthernetLinkSettings EthernetLinkSettingsHolder::create_default() const {
  EthernetLinkSettings settings{};
  settings.remote_ip = m_profile.is_air ? "10.0.0.3" : "10.0.0.2";
  return settings;
}

std::optional<EthernetLinkSettings> EthernetLinkSettingsHolder::impl_deserialize(const std::string &file_as_string) const {
  return openhd_json_parse<EthernetLinkSettings>(file_as_string);
}

std::string EthernetLinkSettingsHolder::imp_serialize(const EthernetLinkSettings &data) const {
  const nlohmann::json tmp = data;
  return tmp.dump(4);
}
