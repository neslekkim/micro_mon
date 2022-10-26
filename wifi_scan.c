// My modified version of wifi scan example

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"

#include <stdio.h>

typedef struct {
    char ssid[24];
    unsigned char mac[6];
    int  channel;
    int  rssi_sum;
    int  times_seen;
}Station_t;

#define MAX_STATIONS 20
Station_t List[MAX_STATIONS];
int num_seen = 0;

int Monitor_index = -1;

void ShowStation(Station_t this)
{
    printf("%-25s ch:%d m:%02x%02x%02x%02x%02x%02x",
        this.ssid, this.channel, this.mac[0],this.mac[1],this.mac[2],this.mac[3],this.mac[4],this.mac[5]);
    printf(" rssi:%5.1f\n",(float)this.rssi_sum/this.times_seen);
}


static int scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
    if (!result) return 0;

    Station_t this;
    memset(&this, 0, sizeof(this));

    strncpy(this.ssid, result->ssid, 23);
    memcpy(this.mac, result->bssid, 6);
    this.channel = result->channel;
    this.rssi_sum += result->rssi;
    this.times_seen += 1;

    if (Monitor_index >= 0){
        if (memcmp(&this, &List[Monitor_index], offsetof(Station_t, rssi_sum)) == 0){
            printf("%3d:",result->rssi);
            int NumHashes = result->rssi+90;
            for (int a=0;a<NumHashes;a++) putchar('#'); // Make a bargraph
            putchar('\n');
            // Also graph the RSSI.
        }
        return 0;
    }

    ShowStation(this);

    int a;
    for (a=0;a<num_seen;a++){
        //printf("memcmp ( %x %x %d",&this, &List[a], offsetof(Station_t, rssi_sum));
        if (memcmp(&this, &List[a], offsetof(Station_t, rssi_sum)) == 0){
            //printf("prev seen at %d\n",a);
            List[a].rssi_sum += result->rssi;
            List[a].times_seen += 1;
            break;
        }
    }
    if (a == num_seen){
        if (a < MAX_STATIONS){
            //printf("Save at %d\n",a);
            List[a] = this;
            num_seen = a+1;
        }else{
            printf("Station list full\n");
        }
    }
    return 0;
}

#include "hardware/vreg.h"
#include "hardware/clocks.h"

int wifi_scan()
{
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_scan_options_t scan_options = {0};

    memset(List, 0, sizeof(List));
    num_seen = 0;
    Monitor_index = -1;

    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
    if (err == 0) {
        printf("\nPerforming wifi scan\n");
    } else {
        printf("Failed to start scan: %d\n", err);
    }

    while(true) {
        if (!cyw43_wifi_scan_active(&cyw43_state)) {
            printf("Scan done\n");
            break;
        }
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for WiFi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        sleep_ms(1);
    }

pick_station:
    printf("\nStation summary:\n");
    for (int a=0;a<num_seen;a++){
        printf("%2d ",a);
        ShowStation(List[a]);
    }

    printf("For monitoring specific, press 0-9\n");
    char c = getchar();
    int index = c-'0';
    if (index >= 0 && index < num_seen){
        // Do a scan, filtering only specific.
        printf("Monitoring: ");
        ShowStation(List[index]);
        Monitor_index = index;

         for (int n=0;n<80;n++){
            cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
            while(true) {
                if (!cyw43_wifi_scan_active(&cyw43_state)) {
                    break;
                }
                // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
                // main loop (not from a timer) to check for WiFi driver or lwIP work that needs to be done.
                cyw43_arch_poll();
                sleep_ms(1);
            }
        }
        goto pick_station;
    }

    return 0;
}
