#include <stdio.h>
#include <memory.h>
#include "one_wire.h"
#include "sensor_remote.h"

#define ONE_WIRE_BUS_PIN 19 // GPIO 18 is pin 24 on the board.

#define MAX_SENSORS 20  // If more than that many 18b20s, ignore some of them.

static rom_address_t sensors[MAX_SENSORS];
static int since_seen[MAX_SENSORS];
static int num_sensors;

//====================================================================================
// Print sensor id in hex.
//====================================================================================
static void PrintSensor(rom_address_t * addr)
{
    unsigned char * addr_sane = (unsigned char *)addr;
    for (int i=6;i>0;i--){
        printf("%02x",addr_sane[i]);
    }
    putchar(' ');
}


//====================================================================================
// When wet, find sensors doesn't always find all, so maintain a list of previously
// seen sensors, update after not seen 10x.
//====================================================================================
static void UpdateSensorList(One_wire * oneWire)
{
    int num_s = oneWire->find_and_count_devices_on_bus();
    my_sleep_ms(0);
    printf("\n1-wire Devices found: %d\n",num_s);

//static int IgnoreMask;
//IgnoreMask += 3;

    for (int n=0;n<num_s;n++){
        rom_address_t addr = oneWire->get_address(n);

//if ((IgnoreMask >> n) & 1){
//  printf("Pretend missing\n");
//  continue;
//}

        // find or insert into array of sensors seen previously
        int a;
        for (a=0;a<num_sensors;a++){
            if (since_seen[a] && memcmp(&addr, &sensors[a], sizeof(addr)) == 0){
                since_seen[a] = 1;
                break;
            }
        }
        if (a >= num_sensors){
            printf("New sensor: "); PrintSensor(&addr);
            if (a < MAX_SENSORS){
                printf("\n");
                sensors[num_sensors] = addr;
                since_seen[num_sensors] = 1;
                num_sensors += 1;
            }else{
                printf(" but too many\n");
            }
        }
    }
    int w=0;
    for (int a=0;a<num_sensors;a++){
        if (since_seen[a] > 10){
            printf("Remove: "); PrintSensor(&sensors[a]); printf("\n");
            continue;
        }
        if (a != w){
            since_seen[w] = since_seen[a];
            sensors[w] = sensors[a];
        }
        since_seen[w] += 1;
        w += 1;
    }
    num_sensors = w;
}

//====================================================================================
// My code to use one_wire.cpp to get a set of readings.
//====================================================================================
extern "C" void ds18b20_read_sesnors(void * arg)
{
    #define MAX_SENSORS 20  // If more than that many 18b20s, ignore some of them.

    One_wire oneWire(ONE_WIRE_BUS_PIN);
    oneWire.init();

    UpdateSensorList(&oneWire);


    my_sleep_ms(0);
    printf("\nTemperature readings:\n");
    for (int a=0;a<num_sensors;a++){
        rom_address_t addr = oneWire.get_address(a);
        unsigned char * addr_sane = (unsigned char *)&addr;
        printf("   %02x%02x", addr_sane[2],addr_sane[1]);
        my_sleep_ms(0);
    }
    printf("\n");

    float temp_sums[MAX_SENSORS];
    int temp_nums[MAX_SENSORS];
    memset(temp_sums, 0, sizeof(temp_sums));
    memset(temp_nums, 0, sizeof(temp_nums));


    // Take several readings each for better accuracy
    const int NUM_AVERAGE = 4;
    for (int nr=0;nr<NUM_AVERAGE;nr++){
        for (int a=0;a<num_sensors;a++){
            rom_address_t addr = oneWire.get_address(a);
            int wait_ms = oneWire.convert_temperature(addr,0,0); // How long conversion *should* take
            my_sleep_ms(0);
        }

        my_sleep_ms(1000); // 500 ms fast enough for brand name devices.
        // Cheap chinese clones need 800 miliseconds to have a conversion ready.

        for (int a=0;a<num_sensors;a++){
            rom_address_t addr = oneWire.get_address(a);
            unsigned char * addr_sane = (unsigned char *)&addr;
            float temp = oneWire.temperature(addr, 0);
            printf(" %6.2f", addr_sane[1], temp);
            if (temp != oneWire.invalid_conversion){
                temp_sums[a] += temp;
                temp_nums[a] += 1;
            }
        }
        printf("\n");
    }

    const int LINE_LEN = 22;
    char ResponseStr[MAX_SENSORS*LINE_LEN+10];
    int str_index = 0;

    for (int line=0;line<num_sensors;line++){
        rom_address_t addr = oneWire.get_address(line);
        unsigned char * addr_sane = (unsigned char *)&addr;
        strcpy(ResponseStr+str_index,"t(");
        str_index += 2;
        for (int i=6;i>0;i--){
            sprintf(ResponseStr+str_index,"%02x",addr_sane[i]);
            str_index += 2;
        }
        float t_avg = -99;
        if (temp_nums[line] > 0) t_avg = temp_sums[line]/temp_nums[line];
        sprintf(ResponseStr+str_index, ")=%6.2f\n  ",t_avg);
        str_index += 9;
    }
    //printf("res=>>%s<<\n",ResponseStr);


    if (arg){
        // Request came from a TCP connection.
        SendResponse(arg, ResponseStr, str_index);
    }
}
