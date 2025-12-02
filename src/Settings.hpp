#pragma once

#include <stdint.h>

struct Settings
{
  char wifiSsid[256];
  char wifiPassword[256]; // only wpa2-psk auth supported
};
