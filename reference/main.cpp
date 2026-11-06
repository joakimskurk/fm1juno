#include <cstdio>

extern "C" {
#include "control106/control_scheduler106.h"
#include "control106/dac106.h"
#include "timer106/timer8253_106.h"
}

int main()
{
    ControlScheduler106 scheduler{};
    control_scheduler106_init(&scheduler);
    const ControlEvent106 first = control_scheduler106_next(&scheduler);

    std::printf("FM1JUNO Phase-1 reference scaffold\n");
    std::printf("first control event=%u, DAC14(0x3fff)->0x%03x, Mode3 N=9 high/low=%u/%u\n",
                static_cast<unsigned>(first.type),
                static_cast<unsigned>(dac106_code14_to_dac12(0x3fff)),
                static_cast<unsigned>(timer8253_106_high_clocks(9)),
                static_cast<unsigned>(timer8253_106_low_clocks(9)));
    return 0;
}
