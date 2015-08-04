#include "uros.h"
#include "system.h"
#include "lib.h"

extern int main();

void start()
{
	extern uint32_t rodata_end;
	extern uint32_t data_start;
	extern uint32_t data_end;

	NVIC_CCR |= 0x200;

	/* Clear SRAM area with zero (Copy .data section bss section is cleared here) */
	memset((void *)SRAM_ADDR, 0, SRAM_SIZE);
	
	/* Copy .data section into SRAM area */
	memcpy((void *)SRAM_ADDR, (void *)&rodata_end, (void *)&data_end - (void *)&data_start);
	
	SYST_RVR = 0x00200000;
	SYST_CSR = 0x00000007;
	
	main();
	
	/* loop forever */
	while (1)
		continue;
}
