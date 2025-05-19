# ESP_Project
ESP web-Server com comunicação Zigbee e controle de válvulas

## MenuConfig
Para buildar o projeto, é necessário seguir os seguintes passos:
no terminal, com o ESP-IDF rodando, digite:

  idf.py menuconfig

Selecione "Serial flasher config" > "Flash size" > 4 MB

Depois, copie o nome do arquivo "personal_partition.csv" dentro da pasta de firmware
selecione a opção "Partition Table" > "Partition Table" > "Custom partition table CSV"
após este passo, surgirá a opção "Partition Table" > "Partition Table" > "Custom partition CSV file".
Pressione enter e cole o nome do arquivo com o caminho relativo "../personal_partition.csv"