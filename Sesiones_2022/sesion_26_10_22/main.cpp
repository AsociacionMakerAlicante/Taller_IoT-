#include "pines.h"
#include <lmic.h>
#include <hal/hal.h>
#include <RocketScream_LowPowerAVRZero.h>
#include <CayenneLPP.h>

//Tamaño de los datos enviados a Cayenne.
//Cada sensor añade al payload 2 bytes + los bytes de sus datos.
CayenneLPP lpp(4); 

// Declaración de funciones.
void wakeUp(void);
void pulsador(void);
void do_send(osjob_t* j);

//Hay un envío en curso, no poner el microcontrolador a dormir.
volatile boolean envioEnCurso = true;

 //Pines del ATmega4808 no utilizados en la aplicación
 //se ponen aquí para reducir el consumo de corriente.

const uint8_t unusedPins[] = {1, 2, 3, 12, 13, 15, 16, 17, 19, 20, 21};

// Device EUI. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "lsb"
static const u1_t PROGMEM DEVEUI[8]={ 0xFF, 0xFF, .... };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// App EUI. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "lsb"
static const u1_t PROGMEM APPEUI[8]={ 0xFF, 0xFF, ....};
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// App Key. Al copiar los datos de la consola de Helium hay que indicarle que lo haga en formato "msb"
static const u1_t PROGMEM APPKEY[16] = { 0xFF, 0xFF, ....};
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

static uint8_t mydata[] = "H";
static osjob_t sendjob;

// Intervalo en segundos entre envios (Pensado para placas que no duermen). El tiempo empieza a contar
// al finalizar la trasmisión. Por ejemplo si tenemos un intervalo de 60 segundos, el primer envío
// se hace a las 12:00:00 y la transmisión dura 2 segundos, el segundo envío se hace a las 12:01:02
//const unsigned TX_INTERVAL = 120; 

// Al compilar aparece un aviso de que no tiene el pinmap de la placa y que debemos utilizar un propio.
// Le damos el pinmap de la placa.
const lmic_pinmap lmic_pins = {
    .nss = RFM95_SS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = RFM95_RST,
    .dio = {RFM95_DIO0, RFM95_DIO1, LMIC_UNUSED_PIN},
};

// Imprime información en hexadecimal en el monitor serial al producirse el evento "Joined"
// (Se ha unido correctamente)
void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial2.print('0');
    Serial2.print(v, HEX);
}

// Función declarada en la libreria lmic. Se ejecuta cada vez que se produce un evento.
// Aquí podemos poner código de control.
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
            }
            //Deshabilitar la validación de comprobación de enlaces (habilitada automáticamente durante la unión, 
            // pero no es compatible con TTN en este momento).
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
            Serial2.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial2.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial2.println(F("Received "));
              Serial2.println(LMIC.dataLen);
              Serial2.println(F(" bytes of payload"));
            }
            // Programar la próxima transmisión.
            //os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            envioEnCurso = false; //Finaliza el envío.
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
            /* No utilizar el monitor serial en este evento. */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial2.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;
         default:
            Serial2.println(F("Unknown event"));
            Serial2.println((unsigned) ev);
            break;
    }
}

// Lanza la transmisión de los datos.
void do_send(osjob_t* j){
    // Comprueba si hay un trabajo TX/RX actual en ejecución
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial2.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepara el envío de datos en el próximo momento posible.
        // Enviamos los datos en el formato de Cayenne.
        lpp.reset();
        lpp.addAnalogInput(1, 25.1);
        //lpp.addDigitalInput(2, 0);
        LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), 0);
        //LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        Serial2.println(F("Packet queued"));
    }
}

void setup() {

    //Reduce el consumo de los pines que no se van a usar en el ATmega4808
    uint8_t index;
    for (index = 0; index < sizeof(unusedPins); index++)
    {
      pinMode(unusedPins[index], INPUT_PULLUP);
      LowPower.disablePinISC(unusedPins[index]);
    }

    //Configuramos la interrupción en el ATmega4808 que produce el pin de Wake del TPL5010
    attachInterrupt(digitalPinToInterrupt(TPL5010_WAKE), wakeUp, RISING);

    //Configuramos la interrupción en el ATmega4808 que produce el pulsador.
    attachInterrupt(digitalPinToInterrupt(PIN_PD2), pulsador, FALLING);
    
    Serial2.begin(115200);
    Serial2.println(F("Starting"));

    // LMIC init
    os_init();
    
    // Restablece el estado MAC. Se descartarán las transferencias de datos de sesión y pendientes.
    LMIC_reset();
    
    // allow much more clock error than the X/1000 default. See:
    // https://github.com/mcci-catena/arduino-lorawan/issues/74#issuecomment-462171974
    // https://github.com/mcci-catena/arduino-lmic/commit/42da75b56#diff-16d75524a9920f5d043fe731a27cf85aL633
    // the X/1000 means an error rate of 0.1%; the above issue discusses using
    // values up to 10%. so, values from 10 (10% error, the most lax) to 1000
    // (0.1% error, the most strict) can be used.
    LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);

    // Sub-band 2 - Helium Network
    LMIC_setLinkCheckMode(0); 

    //Spread Factor y potencia de emisión de la radio.
    LMIC_setDrTxpow(DR_SF7, 14);

    // Iniciar un trabajo (el envío también inicia automáticamente OTAA)
    do_send(&sendjob);

    // ### Preguntar a Jorge si hay que quitarlo ###
    os_setCallback (&sendjob, do_send); //QUITAR

    //Configuración de pines.
    pinMode(LED_BUILTIN, OUTPUT);   //LED
    pinMode(TPL5010_WAKE, INPUT);   //TPL5010 WAKE
    pinMode(TPL5010_DONE, OUTPUT);  //TPL5010 DONE4
    pinMode(BOTON, INPUT);          //Pulsador

    //Generamos el pulso de DONE del TPL5010
    //El TPL5010 comienza a funcionar.
    digitalWrite(TPL5010_DONE, HIGH);
    delay(1);
    digitalWrite(TPL5010_DONE, LOW);
  }

void loop() {
    //Realiza el envío en curso si lo hay.
    os_runloop_once();
    //Ponemos el microcontrolador a dormir.
    if(envioEnCurso == false)
    {
        Serial2.println(F("Microcontrolador a dormir\n"));
        delay(200);
        LowPower.powerDown();
        //Se despierta aquí después de atender a la interrupción del timer.
        Serial2.println(F("Microcontrolador despierto"));
    }      
}

//TIC Timer R=18k -> 36s, R=20k -> 60s.
void wakeUp(void)
{   
    //Generamos la señal de Done para el timer.
    digitalWrite(TPL5010_DONE, HIGH);
    //Añadimos un delay para el tiempo de señal en alto de DONE del TPL5010.
    for(char i = 0; i < 5; i++){} //Ancho de pulso mínimo 100 ns. 8.5 us con i < 3.
    digitalWrite(TPL5010_DONE, LOW); 

    //Preparamos un envío a Helium.
    envioEnCurso = true;  //Se pone en false cuando se finaliza el envío.
    os_setCallback (&sendjob, do_send);
}

// Evento producido cuando se presiona el pulsador.
void pulsador(void)
{   
    
}