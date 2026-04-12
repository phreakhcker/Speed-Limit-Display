// =============================================
// CAPTIVE PORTAL — WiFi Setup for Customers
// =============================================
// When no WiFi credentials are saved (first boot or factory reset),
// the device creates a "SpeedLimit-Setup" WiFi hotspot.
// Connect with your phone, and a setup page opens automatically.
// Enter your WiFi name, password, and TomTom API key.
// The device saves them and reboots to normal mode.
//
// To trigger setup mode again:
//   Hold the BOOT button while pressing RESET.
// =============================================

#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// --- Portal Configuration ---
static const char* AP_SSID = "SpeedLimit-Setup";
static const char* AP_PASS = NULL;  // Open network (no password)
static const int   DNS_PORT = 53;
static const int   WEB_PORT = 80;

// --- NVS Keys for credentials ---
static const char* NVS_NS       = "creds";
static const char* NVS_SSID     = "ssid";
static const char* NVS_PASS     = "pass";
static const char* NVS_SSID2    = "ssid2";
static const char* NVS_PASS2    = "pass2";
static const char* NVS_APIKEY   = "apikey";
static const char* NVS_SETUP    = "setup_done";

// --- Portal State ---
static WebServer  portalServer(WEB_PORT);
static DNSServer  dnsServer;
static bool       portalRunning = false;

// ==============================
// HTML Setup Page
// ==============================
static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Speed Limit Display Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0f0f23;color:#fff;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#1a1a3e;border-radius:16px;padding:32px 28px;max-width:400px;width:100%;box-shadow:0 8px 32px rgba(0,0,0,0.4)}
h1{font-size:1.4em;text-align:center;margin-bottom:4px}
.sub{text-align:center;color:#8888bb;font-size:0.9em;margin-bottom:24px}
label{display:block;font-size:0.85em;color:#a0a0c0;margin-bottom:4px;margin-top:16px}
input{width:100%;padding:12px;border:2px solid #2a2a5e;border-radius:8px;background:#0f0f23;color:#fff;font-size:1em;outline:none;transition:border 0.2s}
input:focus{border-color:#2563eb}
.hint{font-size:0.75em;color:#666;margin-top:4px}
.section{margin-top:20px;padding-top:16px;border-top:1px solid #2a2a5e}
.section-title{font-size:0.9em;color:#2563eb;font-weight:600;margin-bottom:4px}
button{width:100%;padding:14px;background:linear-gradient(135deg,#2563eb,#1d4ed8);color:#fff;border:none;border-radius:10px;font-size:1.05em;font-weight:600;cursor:pointer;margin-top:24px;transition:transform 0.1s}
button:active{transform:scale(0.98)}
.footer{text-align:center;margin-top:16px;font-size:0.75em;color:#555}
.status{display:none;text-align:center;padding:20px;font-size:1.1em}
.status.show{display:block}
.form.hide{display:none}
</style>
</head>
<body>
<div class="card">
<h1>Speed Limit Display</h1>
<p class="sub">Device Setup</p>

<div id="form" class="form">
<div class="section-title">WiFi Network (required)</div>
<label for="ssid">WiFi Name (SSID)</label>
<input type="text" id="ssid" name="ssid" placeholder="Your_Home_WiFi" required>

<label for="pass">WiFi Password</label>
<input type="password" id="pass" name="pass" placeholder="Your WiFi password">
<p class="hint">Leave blank if your network has no password</p>

<div class="section">
<div class="section-title">Second WiFi (optional)</div>
<label for="ssid2">WiFi Name 2</label>
<input type="text" id="ssid2" name="ssid2" placeholder="Phone_Hotspot">

<label for="pass2">Password 2</label>
<input type="password" id="pass2" name="pass2" placeholder="Hotspot password">
<p class="hint">Add your phone hotspot so the device works in the car</p>
</div>

<div class="section">
<div class="section-title">TomTom API Key (required)</div>
<label for="apikey">API Key</label>
<input type="text" id="apikey" name="apikey" placeholder="Your_TomTom_API_Key_Here" required>
<p class="hint">Get a free key at <strong>developer.tomtom.com</strong></p>
</div>

<button onclick="submitForm()">Save &amp; Connect</button>
</div>

<div id="status" class="status">
<p>&#x2705; Settings saved!</p>
<p style="margin-top:8px;color:#8888bb">Device is restarting...</p>
<p style="margin-top:8px;color:#666;font-size:0.85em">Connect to your WiFi network.<br>The device will start showing speed limits.</p>
</div>

<p class="footer">LeeMerie3D &mdash; Speed Limit Display v1.0</p>
</div>

<script>
function submitForm(){
  var s=document.getElementById('ssid').value;
  var a=document.getElementById('apikey').value;
  if(!s){alert('Please enter your WiFi name');return}
  if(!a){alert('Please enter your TomTom API key');return}
  var p=document.getElementById('pass').value;
  var s2=document.getElementById('ssid2').value;
  var p2=document.getElementById('pass2').value;
  var x=new XMLHttpRequest();
  x.open('POST','/save');
  x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');
  x.onload=function(){
    document.getElementById('form').classList.add('hide');
    document.getElementById('status').classList.add('show');
  };
  x.send('ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)+'&ssid2='+encodeURIComponent(s2)+'&pass2='+encodeURIComponent(p2)+'&apikey='+encodeURIComponent(a));
}
</script>
</body>
</html>
)rawliteral";

// ==============================
// Check if credentials exist
// ==============================
bool portalHasCredentials() {
  Preferences p;
  p.begin(NVS_NS, true);  // read-only
  bool done = p.getBool(NVS_SETUP, false);
  String ssid = p.getString(NVS_SSID, "");
  String key = p.getString(NVS_APIKEY, "");
  p.end();
  return done && ssid.length() > 0 && key.length() > 0;
}

// ==============================
// Load credentials from NVS
// ==============================
struct PortalCreds {
  String ssid;
  String pass;
  String ssid2;
  String pass2;
  String apikey;
  int networkCount;
};

PortalCreds portalLoadCreds() {
  PortalCreds c;
  Preferences p;
  p.begin(NVS_NS, true);
  c.ssid   = p.getString(NVS_SSID, "");
  c.pass   = p.getString(NVS_PASS, "");
  c.ssid2  = p.getString(NVS_SSID2, "");
  c.pass2  = p.getString(NVS_PASS2, "");
  c.apikey = p.getString(NVS_APIKEY, "");
  p.end();

  c.networkCount = 1;
  if (c.ssid2.length() > 0) c.networkCount = 2;

  return c;
}

// ==============================
// Clear all saved credentials
// ==============================
void portalClearCreds() {
  Preferences p;
  p.begin(NVS_NS, false);
  p.clear();
  p.end();
  Serial.println("[Portal] Credentials cleared");
}

// ==============================
// Handle form submission
// ==============================
void portalHandleSave() {
  String ssid   = portalServer.arg("ssid");
  String pass   = portalServer.arg("pass");
  String ssid2  = portalServer.arg("ssid2");
  String pass2  = portalServer.arg("pass2");
  String apikey = portalServer.arg("apikey");

  Serial.println("[Portal] Saving credentials...");
  Serial.printf("[Portal]   SSID: %s\n", ssid.c_str());
  Serial.printf("[Portal]   SSID2: %s\n", ssid2.length() > 0 ? ssid2.c_str() : "(none)");
  Serial.printf("[Portal]   API Key: %s...%s\n",
    apikey.substring(0, 4).c_str(),
    apikey.substring(apikey.length() - 4).c_str());

  Preferences p;
  p.begin(NVS_NS, false);
  p.putString(NVS_SSID, ssid);
  p.putString(NVS_PASS, pass);
  p.putString(NVS_SSID2, ssid2);
  p.putString(NVS_PASS2, pass2);
  p.putString(NVS_APIKEY, apikey);
  p.putBool(NVS_SETUP, true);
  p.end();

  portalServer.send(200, "text/plain", "OK");

  Serial.println("[Portal] Credentials saved. Rebooting in 2 seconds...");
  delay(2000);
  ESP.restart();
}

// ==============================
// Start the captive portal
// ==============================
void portalStart(Adafruit_SSD1351 &disp) {
  Serial.println("[Portal] Starting setup hotspot...");

  // Show setup screen on display
  disp.fillScreen(0x0000);
  disp.setTextColor(0xFFFF);
  disp.setCursor(10, 30);
  disp.print("WiFi Setup");
  disp.setTextColor(0x8410);
  disp.setCursor(10, 55);
  disp.print("Connect to:");
  disp.setTextColor(0x07E0);  // Green
  disp.setCursor(10, 75);
  disp.print("SpeedLimit-");
  disp.setCursor(10, 90);
  disp.print("Setup");
  disp.setTextColor(0x8410);
  disp.setCursor(10, 115);
  disp.print("on your phone");

  // Start WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(500);

  Serial.printf("[Portal] Hotspot active: %s\n", AP_SSID);
  Serial.printf("[Portal] IP: %s\n", WiFi.softAPIP().toString().c_str());

  // DNS server redirects ALL domains to our IP (captive portal behavior)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Web server routes
  portalServer.on("/", HTTP_GET, []() {
    portalServer.send_P(200, "text/html", PORTAL_HTML);
  });

  portalServer.on("/save", HTTP_POST, portalHandleSave);

  // Captive portal detection endpoints (phones check these automatically)
  portalServer.on("/generate_204", HTTP_GET, []() {
    portalServer.sendHeader("Location", "http://192.168.4.1/");
    portalServer.send(302);
  });
  portalServer.on("/hotspot-detect.html", HTTP_GET, []() {
    portalServer.sendHeader("Location", "http://192.168.4.1/");
    portalServer.send(302);
  });
  portalServer.on("/connecttest.txt", HTTP_GET, []() {
    portalServer.sendHeader("Location", "http://192.168.4.1/");
    portalServer.send(302);
  });
  portalServer.on("/canonical.html", HTTP_GET, []() {
    portalServer.sendHeader("Location", "http://192.168.4.1/");
    portalServer.send(302);
  });

  // Catch-all: redirect everything to the setup page
  portalServer.onNotFound([]() {
    portalServer.sendHeader("Location", "http://192.168.4.1/");
    portalServer.send(302);
  });

  portalServer.begin();
  portalRunning = true;

  Serial.println("[Portal] Web server started. Waiting for setup...");
}

// ==============================
// Run portal (call in loop)
// ==============================
void portalLoop() {
  if (!portalRunning) return;
  dnsServer.processNextRequest();
  portalServer.handleClient();
}

#endif
