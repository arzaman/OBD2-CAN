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
  - Pinging continuo in broadcast (`0x7DF`) per ottenere valori chiave correnti.
  - Interfaccia grafica multi-schermata via TFT (RPM Analogici, Tachimetro, Griglia Dati).
  - Meccanismo di ripristino automatico hardware dai guasti del bus differenziale (`BUS_OFF recovery`).
  - Indicatore visivo di stato della connessione (Pallino Verde in caso di ACK, Rosso in caso di mancato collegamento o mancati pacchetti in ricezione per un periodo superiore a N secondi).

## 2. Il Simulatore ECU HIL (Hardware-In-The-Loop)
Il firmware "Simulator" agisce come una **Centralina Motore Reale (ECU)**:
- **Hardware**: M5Stack AtomS3 Lite + Unit CAN (Grove CA-IS3050G).
- **Funzionalità**:
  - Ascolto passivo del bus CAN e decodifica dei messaggi di diagnostica.
  - Simulatore fisico con modello vettoriale dinamico al variare del tempo (Motore, Trasmissione, Acqua).
  - Funzione "Acceleratore": L'integrazione del pulsante centrale dell'AtomS3 Lite funge da gas permettendo di simulare ramp-up a 6500 RPM, accelerazioni fino a 180 km/h e salita del carico motore e temperature in tempo reale.
  - Risposta sincrona al Master simulando un ID veicolare standard (`0x7E8`).

## Cablaggio Fisico
- **Dispositivi**: 2 x Controller (uno scanner e un simulatore) sui rispettivi moduli transceiver isolati.
- **Protocollo**: CAN-Bus differenziale nativo a `500 kbps`.
- **Terminazione**: Assicurarsi che i pin terminali `CAN_H` e `CAN_L` siano uniti da una resistenza da **120 Ohm** per garantire l'adattamento delle linee differenziali ed evitare bus error (`TX_Failed`) causati da segnali riflessi o impedenza fluttuante.

## Licenza
Progetto per usi interni e di testing.
