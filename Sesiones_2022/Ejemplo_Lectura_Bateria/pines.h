/*
*Asignación hardware de los pines del microcontrolador.
*/
#ifndef PINES_H
#define PINES_H

#include <Arduino.h>
#define VERSION_1 //Comentar esta línea para la placa usada en el Maker.

// Definición de los pines asignados en el hardware.
//TPL5010 Timer
#define TPL5010_DONE PIN_PC0
#if defined(VERSION_1)
#define TPL5010_WAKE PIN_PC1
#else
#define TPL5010_WAKE PIN_PC2
#endif

//RFM95 Radio LoRa
#if defined(VERSION_1)
#define RFM95_DIO0 PIN_PC2
#else
#define RFM95_DIO0 PIN_PC1
#endif
#define RFM95_DIO1 PIN_PC3
#define RFM95_RST PIN_PD6
#define RFM95_SS PIN_PA7
#define RFM95_SCK PIN_PA6
#define RFM95_MISO PIN_PA5
#define RFM95_MOSI PIN_PA4
//LED en el PCB
#define LED_BUILTIN PIN_PA0

#endif