#include <Arduino.h>
#include <pines.h>
#include <lmic.h>
#include <hal/hal.h>
#include <RocketScream_LowPowerAVRZero.h>

// 
void wakeUp(void);
void onEvent (ev_t ev);
void pulsador(void);
void do_send(osjob_t* j);

volatile boolean envioEnCurso = false;

/***********************************
 * Pines del ATmega4808 no utilizados en la aplicación
 * se ponen aquí para reducir el consumo de corriente.
 **********************************/
const uint8_t unusedPins[] = {1, 2, 3, 12, 13, 15, 16, 17, 19, 20, 21};

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]={0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8]={ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.

static const u1_t PROGMEM APPKEY[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

static uint8_t mydata[] = "Hola, mundo!";
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60; 

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = RFM95_SS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = RFM95_RST,
    .dio = {RFM95_DIO0, RFM95_DIO1, LMIC_UNUSED_PIN},
};


void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial2.print('0');
    Serial2.print(v, HEX);
}

void onEvent (ev_t ev) {
    Serial2.print(os_getTime());
    Serial2.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial2.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial2.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial2.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial2.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial2.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial2.println(F("EV_JOINED"));
            {//
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial2.print("netid: ");
              Serial2.println(netid, DEC);
              Serial2.print("devaddr: ");
              Serial2.println(devaddr, HEX);
              Serial2.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial2.print("-");
                printHex2(artKey[i]);
              }
              Serial2.println("");
              Serial2.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial2.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial2.println();
            }//
            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0); //0
            break;
        case EV_RFU1:
            Serial2.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial2.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial2.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            envioEnCurso = false;
            Serial2.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial2.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial2.println(F("Received "));
              Serial2.println(LMIC.dataLen);
              Serial2.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial2.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial2.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial2.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial2.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial2.println(F("EV_LINK_ALIVE"));
            break;
        case EV_TXSTART:
            Serial2.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial2.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial2.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;
         default:
            Serial2.println(F("Unknown event"));
            Serial2.println((unsigned) ev);
            break;
    }
    envioEnCurso = false;
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial2.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        Serial2.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {

    /* Ensure unused pins are not floating */
    //Reduce el consumo de los pines que no se van a usar en el ATmega4808
    uint8_t index;
    for (index = 0; index < sizeof(unusedPins); index++)
    {
      pinMode(unusedPins[index], INPUT_PULLUP);
      LowPower.disablePinISC(unusedPins[index]);
    }

    //Configuramos la interrupción en el ATmega4808 que produce el pin de Wake del TPL5010
    attachInterrupt(digitalPinToInterrupt(TPL5010_WAKE), wakeUp, CHANGE);

    //Configuramos la interrupción en el ATmega4808 que produce el pulsador.
    attachInterrupt(digitalPinToInterrupt(PIN_PD2), pulsador, FALLING);
    
    Serial2.begin(115200);
    Serial2.println(F("Starting"));

    #ifdef VCC_ENABLE
    // For Pinoccio Scout boards
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
    #endif

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    
    // allow much more clock error than the X/1000 default. See:
    // https://github.com/mcci-catena/arduino-lorawan/issues/74#issuecomment-462171974
    // https://github.com/mcci-catena/arduino-lmic/commit/42da75b56#diff-16d75524a9920f5d043fe731a27cf85aL633
    // the X/1000 means an error rate of 0.1%; the above issue discusses using
    // values up to 10%. so, values from 10 (10% error, the most lax) to 1000
    // (0.1% error, the most strict) can be used.
    LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);

    // Sub-band 2 - Helium Network
    LMIC_setLinkCheckMode(0); //0
    LMIC_setDrTxpow(DR_SF7, 14);//7,14

    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);
    os_setCallback (&sendjob, do_send); //QUITAR

     //Configuración de pines, usar #defines en lugar de número mágicos.
    pinMode(LED_BUILTIN, OUTPUT); //LED
    pinMode(TPL5010_WAKE, INPUT); //TPL5010 WAKE
    pinMode(TPL5010_DONE, OUTPUT); //TPL5010 DONE4
    pinMode(PIN_PD2, INPUT); //Pulsador

    //Generamos el pulso de DONE del TPL5010
    //El TPL5010 comienza a funcionar.
    digitalWrite(TPL5010_DONE, HIGH);
    delay(1);
    digitalWrite(TPL5010_DONE, LOW);

    
  }

void loop() {
    //El programa continua aquí cada vez que se despierta de la ISR. 
    //Preparamos un envío a The Things Network.
    //envioEnCurso = true;  //Se pone en false cuando se finaliza el envío.
    //os_setCallback (&sendjob, do_send); 
    //while (envioEnCurso)
    //{
      os_runloop_once();
    //}
    
    //Microcontrolador a dormir después de procesar la ISR del timer.
    //LowPower.powerDown();
}

//TIC Timer 18k -> 36s, 20k -> 60s.
void wakeUp(void)
{   
    digitalWrite(TPL5010_DONE, HIGH);
    //Añadimos un delay para el tiempo de señal en alto de DONE del TPL5010.
    for(char i = 0; i < 5; i++){} //Ancho de pulso mínimo 100 ns. 8.5 us con i < 3.
    digitalWrite(TPL5010_DONE, LOW); 
}

void pulsador(void)
{   
    //Preparamos un envío a The Things Network.
    //envioEnCurso = true;  //Se pone en false cuando se finaliza el envío.
    //os_setCallback (&sendjob, do_send); 
}
