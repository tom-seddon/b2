#include <shared/system.h>
#include <shared/testing.h>
#include <beeb/6522.h>

int main(void) {
    {
        R6522::PCR pcr;
        pcr.value=1;

        TEST_TRUE(pcr.bits.ca1_pos_irq);
    }

    {
        R6522::ACR acr;
        acr.value=1;

        TEST_TRUE(acr.bits.pa_latching);
    }

    {
        R6522::IRQ irq;
        irq.value=1;

        TEST_TRUE(irq.bits.ca2);
    }
}
