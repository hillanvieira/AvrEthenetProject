# AvrEthenetProject

Control outputs over ethernet protocol
 
### Detalhes do Projeto

Projeto feito com um atmega328P usando a ide Atmel sutudio em lingguegem C.
para prototipagem foi utilizado ISIS PROTEUS.
para o webapp foi utilasado AngularJS

### Funcinamento

quano o usuario acessa o sistema via ip pelo navegador é carregado uma pagina feita em AngularJS que fica salva num SD card conectado ao micro controlado via protocolo spi
onde é possivel ver e mudar o estado das saidas e poder monitorar valores de sensores conectador nas entradas adc.
Para fazer a interface de rede é usado W5500 TCP/IP Ethernet controlle conectado também via protocolo spi a MCU

### Aplicativo Android.

https://play.google.com/store/apps/details?id=br.com.hillanmec.easyswitch

no link acima o aplicativo que desenvolvi que permite controlar as saidas, configurar senha de acesso e mudar as configuracões de rede.


