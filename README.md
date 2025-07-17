# Escape Room Distribuita (Reti Informatiche 2023/2024)
Il progetto, sviluppato in C per l’esame di Reti Informatiche all’Università di Pisa, realizza un sistema distribuito su TCP per un gioco testuale di Escape Room.

## Descrizione
Un sistema client‑server distribuito che riproduce un gioco testuale di Escape Room. I processi comunicano via TCP, utilizzando protocolli “text” per comandi e messaggi, e “binary” per codici di stato e lunghezze.

- **Main Server**  
  - Punto di ingresso unico per registrazione, login/logout  
  - Gestione delle credenziali in `database.txt` (username + password criptata)  
  - Creazione e terminazione di Game Server tramite `fork()` e segnali (`start <port>`, `stop`)  
  - I/O multiplexing (gestione concorrente di più socket)

- **Game Server**  
  - Ospita un’Escape Room (codici disponibili: `0` = Medievale, `1` = Antico Egitto)  
  - I/O multiplexing per gestire più giocatori in tempo reale  
  - Partenza della partita al `begin` di ≥2 giocatori, con timer di sessione  
  - Comandi testuali per muoversi, interagire con oggetti ed enigmi, invio periodico di aggiornamenti (“timeout”)  
  - Scambio di messaggi tra giocatori

- **Client**  
  - CLI testuale per registrazione/login e selezione del Game Server  
  - Partecipazione all’Escape Room: invio comandi, ricezione di descrizioni, punteggi e messaggi  
  - “Timeout” periodico per aggiornamenti real‑time

## Come compilare
Ci sono il Makefile e uno script exec2024.sh. 
