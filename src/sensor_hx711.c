
#include <string.h> // memcpy
#include "autoconf.h" //
#include "basecmd.h" //oid_alloc
#include "command.h"  //sendf
#include "sched.h" //DECL_COMMAND
#include "board/gpio.h" //GPIO/read/setup
#include "board/misc.h" // timer_read_time

struct hx711_s {
    struct gpio_out sck_out;
    struct gpio_in dt_in;
    long weight_tare;
    long pulse_count;
};

long HX711_Read(struct hx711_s *dev);
void HX711_Get_WeightTare(struct hx711_s *dev);
long HX711_Get_Weight(struct hx711_s *dev);

void command_config_hx711(uint32_t *args)
{
    struct hx711_s *hx711 = oid_alloc(args[0], command_config_hx711, sizeof(*hx711));
    hx711->sck_out = gpio_out_setup(args[1], 1);
    hx711->dt_in = gpio_in_setup(args[2], 1);
    gpio_out_write(hx711->sck_out, 0);

    hx711->weight_tare = 0;
}
DECL_COMMAND(command_config_hx711, "config_hx711 oid=%c sck_pin=%u dout_pin=%u");

struct hx711_s * hx711_oid_lookup(uint8_t oid)
{
    return oid_lookup(oid, command_config_hx711);
}

void command_read_hx711(uint32_t * args)
{
    static int s_nCnt = 0;
    s_nCnt++;

    uint8_t oid = args[0];
    struct hx711_s *dev = hx711_oid_lookup(args[0]);
    dev->pulse_count = args[1];
    uint8_t data_len = 4;
    uint8_t data[data_len];

    long weight = HX711_Get_Weight(dev);
    data[0] = weight & 0xFF;
    data[1] = (weight>>8) & 0xFF;
    data[2] = (weight>>16) & 0xFF;
    data[3] = (weight>>24) & 0xFF;
    sendf("read_hx711_response oid=%c response=%*s", oid, data_len, data);
}
DECL_COMMAND(command_read_hx711, "read_hx711 oid=%c read_len=%u");

// microsecond delay helper
static inline void hx711_udelay(uint32_t usecs)
{
    uint32_t end = timer_read_time() + timer_from_us(usecs);
    while (!timer_is_before(end, timer_read_time()));
}

// read data from HX711
long HX711_Read(struct hx711_s *dev)
{
    gpio_out_write(dev->sck_out, 0);
    hx711_udelay(1);

    // wait dout to low.
    int nCnt = 0;
    while (gpio_in_read(dev->dt_in))
    {
        hx711_udelay(1);
        if (nCnt++ > 100 * 1000)
            return 0;
    }

    // read 24bit data.
    unsigned long count=0;
    for (int i = 0; i < 24; i++)
    {
        gpio_out_write(dev->sck_out, 1);
        hx711_udelay(1);

        count = count << 1;

        gpio_out_write(dev->sck_out, 0);
        hx711_udelay(1);

        if(gpio_in_read(dev->dt_in))
            count++;
    }

    // last clk, total 25/26/27
    int n = 1;
    if( dev->pulse_count==26 )
        n = 2;
    else if( dev->pulse_count==27 )
        n = 3;

    for (int i = 0; i < n; i++)
    {
        gpio_out_write(dev->sck_out, 1);
        hx711_udelay(1);
        gpio_out_write(dev->sck_out, 0);
        hx711_udelay(1);
    }

    count ^= 0x800000;
    return count;
}

//set weight tare.
void HX711_Get_WeightTare(struct hx711_s* dev)
{
    dev->weight_tare = HX711_Read(dev);
}

//get weight
long HX711_Get_Weight(struct hx711_s* dev)
{
    static int s_nCnt = 0;

    long value = HX711_Read(dev);

    if( s_nCnt<2 ) //reset weight tare at 1/2 times.
    {
        s_nCnt++;
        dev->weight_tare = value;
    }

    return value - dev->weight_tare;
}
