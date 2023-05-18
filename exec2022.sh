# 1. COMPILAZIONE
# Il comando 'make' necessita del makefile, che deve essere
# creato come descritto nella guida sulla pagina Elearn

  make clean
  read -p "make clean eseguita. Premi invio per eseguire..."
  make

  read -p "Compilazione eseguita. Premi invio per eseguire..."

# 2. ESECUZIONE
# I file eseguibili di server e device devono
# chiamarsi 'server' e 'device', e devono essere nella current folder

# 2.1 esecuzioe del server sulla porta 4242
  gnome-terminal -x sh -c "./server 4242; #exec bash"

# 2.2 esecuzione di 3 device sulle porte {5001,...,5003}
  for port in {5001..5003}
  do
     gnome-terminal -x sh -c "./device $port; #exec bash"
  done
