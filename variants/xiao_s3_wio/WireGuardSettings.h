#pragma once

#if defined(WIREGUARD_ENABLED)

#ifndef WIFI_SSID
  #error "WIFI_SSID must be defined when WIREGUARD_ENABLED is set"
#endif

#ifndef WIFI_PWD
  #error "WIFI_PWD must be defined when WIREGUARD_ENABLED is set"
#endif

#ifndef WG_LOCAL_IP
  #define WG_LOCAL_IP "10.0.0.2"
#endif

#ifndef WG_PRIVATE_KEY
  #define WG_PRIVATE_KEY "REPLACE_WITH_BASE64_PRIVATE_KEY"
#endif

#ifndef WG_PEER_PUBLIC_KEY
  #define WG_PEER_PUBLIC_KEY "REPLACE_WITH_BASE64_PEER_PUBLIC_KEY"
#endif

#ifndef WG_ENDPOINT_ADDRESS
  #define WG_ENDPOINT_ADDRESS "vpn.example.com"
#endif

#ifndef WG_ENDPOINT_PORT
  #define WG_ENDPOINT_PORT 51820
#endif

#ifndef WG_NTP_SERVER
  #define WG_NTP_SERVER "pool.ntp.org"
#endif

#ifndef WG_NTP_TIMEOUT_MS
  #define WG_NTP_TIMEOUT_MS 15000
#endif

#endif
