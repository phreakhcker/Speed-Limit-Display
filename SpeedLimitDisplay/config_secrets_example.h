// =============================================
// EXAMPLE CREDENTIALS FILE — COPY THIS TO GET STARTED
// =============================================
//
// HOW TO USE:
//   1. Copy this file and rename the copy to: config_secrets.h
//   2. Fill in your real WiFi networks and TomTom API key
//   3. The real config_secrets.h is ignored by git so your secrets stay private
//
// HOW TO GET A TOMTOM API KEY (free):
//   1. Go to https://developer.tomtom.com
//   2. Create a free account
//   3. Go to Dashboard > My Apps > Add New App
//   4. Copy the API key — the free tier gives you 2,500 requests/day
//
// =============================================

#ifndef CONFIG_SECRETS_H
#define CONFIG_SECRETS_H

// --- WiFi Networks ---
// Add up to 5 networks. The device scans and connects to the first one found.
const int WIFI_NETWORK_COUNT = 2;

const char* WIFI_NETWORKS[][2] = {
  // { "Network Name",        "Password" }
  { "Your_Home_WiFi",         "your_home_password" },
  { "Your_Phone_Hotspot",     "your_hotspot_password" },
};

// --- TomTom API Key ---
const char* TOMTOM_API_KEY = "Your_TomTom_API_Key_Here";

#endif
