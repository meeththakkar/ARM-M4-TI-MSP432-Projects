#include "driverlib.h"
#include "msp.h"

#define  BANK1_START_ADDRESS  0x00020000
#define  FOUR_KB_LENGTH   0x000001000

	uint8_t fill_data[FOUR_KB_LENGTH];
	uint32_t start_address = BANK1_START_ADDRESS;
	uint32_t index;
	uint32_t sector_number = 1;
	volatile bool waitfor1MinuteToComplete;

	long i;
	/* Data */
	void redLEDfor1Second(void);
	void delay(int ms);
	void resetWatchdog();
	void greenLEDfor1Second();

int main(void) {
	//first thing hold timer while we setup the system.
	WDT_A_holdTimer();

	//set clock speed and power level.
	CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);
	MAP_CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
	PCM_setCoreVoltageLevel(PCM_VCORE0);

	//red led for one second.
	redLEDfor1Second();
	//IO config


	P1DIR &= ~BIT4;			// make P1.4 as  digital input ping
	P1REN |= BIT4;
	P1OUT |= BIT4;

	//set flash wait state for faster programming of flash.
	FlashCtl_setWaitState(FLASH_BANK0, 2);
	FlashCtl_setWaitState(FLASH_BANK1, 2);

	//generate fake data to fill inside each sector.
	memset(fill_data, 0x23, FOUR_KB_LENGTH);

	//disable all interrupts for temporaty.
	Interrupt_disableMaster();


	//initialize timer using 48Mhz clock and stop it temporarily.
	Timer32_initModule(TIMER32_0_MODULE, TIMER32_PRESCALER_1, TIMER32_32BIT,
			TIMER32_PERIODIC_MODE);
	Timer32_haltTimer(TIMER32_0_MODULE);

	//initialize watchdog for 1 second.
	MAP_CS_setReferenceOscillatorFrequency(CS_REFO_32KHZ);
	MAP_CS_initClockSignal(CS_SMCLK, CS_REFOCLK_SELECT, CS_CLOCK_DIVIDER_1);
	WDT_A_initWatchdogTimer(WDT_A_CLOCKSOURCE_SMCLK, WDT_A_CLOCKITERATIONS_32K);
	//ask watchdog to do hard reset once it expires.
	WDT_A_setTimeoutReset(WDT_A_HARD_RESET);


	//selectively enable all the maskable interrupts interrupts.
	Timer32_enableInterrupt(TIMER32_0_MODULE);
	Interrupt_enableInterrupt(INT_T32_INT1);
	Interrupt_enableMaster();

	while (1) {

		//set timebomb of 1 minute resetISR
		// 2880000000
		// for one minute 					   2880000000
		Timer32_initModule(TIMER32_0_MODULE, TIMER32_PRESCALER_1, TIMER32_32BIT,
				TIMER32_PERIODIC_MODE);
		Timer32_haltTimer(TIMER32_0_MODULE);
		Timer32_setCount(TIMER32_0_MODULE, 2880000000);
		Timer32_startTimer(TIMER32_0_MODULE, true);

		//start watchdog
		WDT_A_startTimer();

		//set status flag for one minute to true .... whe wile loop will be repeated when this will be set to false.
		waitfor1MinuteToComplete = true;

		//run unprotect , earase, write and protect code for 32 sectors of 4 kb which totals to 128 Kb
		sector_number = 1;
		start_address = BANK1_START_ADDRESS;

		for (index = 0; index < 32; ++index) {


			FlashCtl_unprotectSector(FLASH_MAIN_MEMORY_SPACE_BANK1,
					sector_number);

			resetWatchdog();

			FlashCtl_eraseSector(start_address);
			resetWatchdog();
			while (!MAP_FlashCtl_programMemory(fill_data, (void*) start_address,
					(int) FOUR_KB_LENGTH))
				;
			resetWatchdog();

			start_address += FOUR_KB_LENGTH;

			FlashCtl_protectSector(FLASH_MAIN_MEMORY_SPACE_BANK1,
					sector_number);
			//sector_number has to be doubled.
			sector_number = sector_number << 1;
			//snooze watchdog
			resetWatchdog();
		}

		//check if everything is written correctly in flash  or not.
		//writing is complete green lde for 1 second.
		greenLEDfor1Second();


		//wait for one minite to complete and keep resetting watchdog.
		while (waitfor1MinuteToComplete) {
			resetWatchdog();
		}

	}

}

void greenLEDfor1Second() {
	/* green LED for 1 second */
	/* set pins as output for port 2  see pingout for gpio */
	MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN1);
	MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN1);
	resetWatchdog();
	//0.5 second delay ...
	delay(500);

	resetWatchdog();
	//0.5 sec
	delay(500);

	//turn off green..
	MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN1);
	resetWatchdog();

}

void redLEDfor1Second(void) {

	/* red LED for 1 second */
	/* set pins as output for port 2  see pingout for gpio */
	GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0);

	//0.5 second delay ...
	delay(1000);
	//turn off red..
	MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0);

}

void delay(int ms) {

	//32  cycles for +ve loop.
	long cycels = ms * 48000 / 36;

	for (i = 0; i < cycels; i++) {
	}
}

/*
 * ISR for timer of 1 minute which sets the flag to false.
 */
void timer_0_ISR(void) {

	Timer32_clearInterruptFlag(TIMER32_0_MODULE);
	waitfor1MinuteToComplete = false;

}


// reset watchdog if button is not pressed.
void resetWatchdog() {

	if ((P1IN & BIT4) == 0) {
		__nop();
		return;
	}
	WDT_A_clearTimer();

}
