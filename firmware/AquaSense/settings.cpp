#include "settings.h"

#include <Preferences.h>

#include "config.h"

static const char *kNamespace = "aquasense";

static void load_string(Preferences &prefs, const char *key, char *out, size_t size) {
  if (prefs.isKey(key)) {
    prefs.getString(key, out, size);
  }
}

void Settings::load() {
  post_interval_s = DEFAULT_POST_INTERVAL_S;
  Preferences prefs;
  prefs.begin(kNamespace, true);
  load_string(prefs, "ssid", ssid, sizeof(ssid));
  load_string(prefs, "password", password, sizeof(password));
  load_string(prefs, "server", server, sizeof(server));
  load_string(prefs, "token", token, sizeof(token));
  load_string(prefs, "device_id", device_id, sizeof(device_id));
  post_interval_s = prefs.getUShort("interval", post_interval_s);
  prefs.end();
}

void Settings::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.putString("server", server);
  prefs.putString("token", token);
  prefs.putString("device_id", device_id);
  prefs.putUShort("interval", post_interval_s);
  prefs.end();
}

void Settings::clear() {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.clear();
  prefs.end();
  *this = Settings();
  post_interval_s = DEFAULT_POST_INTERVAL_S;
}

bool valid_device_id(const char *id) {
  if (!id[0]) return false;
  for (const char *p = id; *p; p++) {
    if (!isalnum(static_cast<unsigned char>(*p)) && *p != '-' && *p != '_' && *p != '.') {
      return false;
    }
  }
  return true;
}
