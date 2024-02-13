# Progetto: ESCAPE ROOM
# Autore: Francesco Panattoni
# Matricola: 604230

# run

# Compilazione
make

read -p "Compilazione eseguita. Premi invio per eseguire..."

# Avvia il server in una nuova finestra del terminale
gnome-terminal -x sh -c "./server 4242; exec bash"

# Avvia due client in nuove finestre del terminale
gnome-terminal -x sh -c "./client 4242; exec bash"
gnome-terminal -x sh -c "./client 4242; exec bash"