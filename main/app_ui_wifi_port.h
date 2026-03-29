#ifndef __APP_UI_WIFI_PORT_H__
#define __APP_UI_WIFI_PORT_H__

void ui_wifi_port_wifi_enable(void);
void ui_wifi_port_wifi_disable(void);
void ui_wifi_port_wifi_scan(void);
void ui_wifi_port_wifi_connect(const char *ssid, const char *password);
void ui_wifi_port_wifi_disconnect(void);
void ui_wifi_port_add_wifi_card(const char *wifi_name, int signal_strength);

#endif
