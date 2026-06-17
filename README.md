# OBD2-CAN Simulator and Scanner (M5Stack)

Un progetto per la lettura e la simulazione Hardware-in-the-Loop (HIL) di telemetrie veicolari OBD-II, progettato per funzionare in due ambienti indipendenti su architettura **ESP32-S3** sfruttando l'ecosistema **M5Stack**.

## Struttura del Progetto (PlatformIO)
Il codice sorgente è unico, ma gestito in due environment separati via `platformio.ini`:
1. `[env:scanner]`
2. `[env:simulator]`

Questa infrastruttura garantisce il corretto isolamento del firmware, pur mantenendo logiche condivise in `/lib` per l'hardware abstraction del controller CAN (TWAI).

## 1. Lo Scanner OBD2
Il firmware "Scanner" agisce da **Master / Client diagnostico**:
- **Hardware**: M5Stack AtomS3 + Atomic CAN Base (CA-IS3050G).
- **Funzionalità**:
  - **Auto-Discovery del protocollo**: Rilevamento automatico e connessione a veicoli con ID Standard (11-bit) o Extended (29-bit).
  - Pinging continuo in broadcast (`0x7DF` o `0x18DB33F1`) per ottenere valori chiave correnti.
  - Interfaccia grafica multi-schermata via TFT (RPM Analogici, Tachimetro, Griglia Dati).
  - Meccanismo di ripristino automatico hardware dai guasti del bus differenziale (`BUS_OFF recovery`).
  - Indicatore visivo di stato della connessione (Pallino Verde in caso di ACK, Rosso in caso di mancato collegamento o mancati pacchetti in ricezione per un periodo superiore a N secondi).

## 2. Il Simulatore ECU HIL (Hardware-In-The-Loop)
Il firmware "Simulator" agisce come una **Centralina Motore Reale (ECU)**:
- **Hardware**: M5Stack AtomS3 Lite + Unit CAN (Grove CA-IS3050G).
- **Funzionalità**:
  - Ascolto passivo del bus CAN e decodifica dei messaggi di diagnostica.
  - **Gestione 11-bit e 29-bit**: Modalità di emulazione selezionabile dinamicamente tramite **doppio-click** rapido sul pulsante.
  - **Feedback Visivo LED**: Il LED RGB integrato (WS2812C) indica la modalità in uso (🔵 Blu = 11-bit, 🟢 Verde = 29-bit).
  - **Motore Fisico Ibrido**: Risposta dinamica per i Giri Motore (RPM) basata sull'input dell'utente, mentre gli altri parametri diagnostici sono stati bloccati per offrire un ambiente di test stabile e prevedibile.
  - **Funzione "Acceleratore"**: Il pulsante centrale dell'AtomS3 Lite controlla esclusivamente i giri motore, permettendo di simulare ramp-up da 800 a 6500 RPM (PID 0x0C).
  - **Valori Sensori Statici Supportati**:
    - Velocità: `0 km/h` (PID 0x0D)
    - Carico Motore: `20%` (PID 0x04)
    - Temp. Acqua: `80 °C` (PID 0x05)
    - Livello Carburante: `80%` (PID 0x2F)
    - Posizione Farfalla: `20%` (PID 0x11)
  - Risposta sincrona al Master simulando un ID veicolare standard o esteso.

## Sicurezza USB Nativa (CDC)
I firmware includono un sistema di sicurezza per l'USB Nativa (HWCDC). Il logging rileva se il Monitor Seriale su PC è attivo: in caso di disconnessione, i log vengono soppressi per evitare il blocco dei buffer USB e i conseguenti crash causati dal Watchdog Timer, garantendo un'operatività continua in modalità "standalone".

## Cablaggio Fisico
- **Dispositivi**: 2 x Controller (uno scanner e un simulatore) sui rispettivi moduli transceiver isolati.
- **Protocollo**: CAN-Bus differenziale nativo a `500 kbps`.
- **Terminazione**: Assicurarsi che i pin terminali `CAN_H` e `CAN_L` siano uniti da una resistenza da **120 Ohm** per garantire l'adattamento delle linee differenziali ed evitare bus error (`TX_Failed`) causati da segnali riflessi o impedenza fluttuante.

## Licenza
Progetto per usi interni e di testing.
