#include <Arduino.h>
#include "FS.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <DHTesp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include <EEPROM.h> 
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFiManager.h>
#include "ThingSpeak.h"



/* Primera version con variable prueba para cambiar de version usando el mismo programa. Con prueba en 1 se usa
D7 en vez de D8 para accionar la valvula, se usa el token del bot de la pavita en vez de greendruids, no se inicializa
D7 como entrada para no interferir, y se desactivan las llamadas a enviar data a thingspeak */

boolean prueba=1; //En 1 para usar con prototipo de casa, 0 para compilar y cargar en tigre.

#define valvulariego prueba ? GPIO_NUM_33 : GPIO_NUM_32
//ahora elige el bot de la pavita si prueba==1 o el del Greendruids
#define BOT_TOKEN prueba ? "1986916463:AAGl7t1EA_7PcJwnmAkC3VR10tgrtYstTcs" : "1722992359:AAF8w3-tUygSiD5ZWK6f3IYvkJHzGzj8unE" 
//Tb elije cuenta de thingspeak para prueba o la de thaumat
#define API_KEY prueba ?  "34SIENKSTC193Z1T" : "QLK8PFUMTVMF89RG"
#define LUZ_IN GPIO_NUM_27
#define DHT_INT_PIN GPIO_NUM_15
#define DHT_EXT_PIN GPIO_NUM_16
WiFiManager wifiManager;


#define version 100 //version de soft 


byte indice=0;
byte clienteactual; //LLeva registro del numero de cliente de telegram que envio el ultimo mensaje al bot
unsigned long timeuniversal;
const long utcOffsetInSeconds = -10800; // GMT -3 (-3x60x60) 
//char daysOfTheWeek[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
unsigned long BOT_MTBS = 1000; // mean time between scan messages
String cadena;
String text;
WiFiUDP ntpUDP;
WiFiClientSecure secured_client; 
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; // last time messages' scan has been done
DHTesp dht;
DHTesp dht2;
/** Data from sensor 1 */
TempAndHumidity sensor1Data;
/** Data from sensor 2 */
TempAndHumidity sensor2Data;
Adafruit_BMP280 bme;
const char* server = "api.thingspeak.com";
String apiKey =API_KEY;
unsigned long tiempoentremuestras, tiempoultimamuestra;
unsigned long ultimochequeoalarma, tiempochequeoalarma;
byte numNewMessages;
float datos[8]; //arrays para almacenar los datos y registrar minimos y maximos de todos los parametros
float datosmin[8];
float datosmax[8];
byte TempintalarMin; // alarmas de maximo y minimo para ser enviadas por telegram
byte TempintalarMax;
byte alarmasporenviar; //contador de mensajes de alarma a ser enviados por el bot
int sueloalarMin; // valor de humedad de suelo minima para que se envie una alarma
byte numparametros=7; //El numero de variables a registrar segun sensores instalados
unsigned long delaythingspeak=16000; //Se puede enviar una muestra cada 15 segundos nomas.
unsigned long thingspeakwaitTimer;
boolean thingspeakwaitflag=0; //Para esperar 15 segundos entre subida de una muestra y otra x limitacion de la pagina
unsigned long timermaxmin; //Registra marca de tiempo desde el ultimo reseteo de maximos y minimos.
unsigned int periodomaxmin; //Tiempo en minutos para relevar max y min luego de lo cual es reseteado.
unsigned long timerled; //periodoled=3000;
unsigned long periodoriego; //Tiempo que dura cada operacion de riego iniciada desde el bot
unsigned long timer_riego; 
unsigned long periodo_luz, timerluz; // Intervalo maximo de tiempo que puede estar sin luz sin detectar alarma
unsigned long marcador_riego; //Registra el momento en que termina de regar para medir el tiempo desde el ultimo riego
byte numerodemenu=0; //Para que el bot sepa donde esta dentro del arbol, siendo cero el nivel raiz y los demas numeros asignados a las distintas opciones
byte temp;
String chat_id;
#define numclientes 4 //numero maximo de clientes de telegram registrados
boolean isvalidnum; //Si el mensaje del bot contiene un argumento numerico valido
boolean flagalarma=1; //Habilita el envio de alarmas, si esta en cero no se envia ninguna
boolean flagled; //Para el led interno
boolean estadoriego;
boolean riegoencurso;
boolean stateluz, laststateluz; //Estados de la iluminacion para detectar flancos y enviar alarma por falla
unsigned int argumentonumerico;  //La parte numerica del mensaje de telegram convertido a entero
byte estadoalarma; //Bit 1 es alarma x alta temp, Bit2 baja temp, Bit 3 Poca humedad suelo, Bit 4 corte de luz interno, Bit 5 alarma por luz
byte tiemporiego; //Duracion de la secuencia de riego cuando se inicia por comando Startriego
byte maskalarma=255;  //Para inhabilitar independientemente. Bit 1 es alarma x alta temp, Bit2 baja temp, Bit 3 Poca humedad suelo, Bit 4 corte de luz interno, Bit 5 alarma por luz
boolean flagfallaled;
boolean firstriego; //Se setea al regar por primera vez para evitar que aparzcan X minutos desde el ultimo riego sin haber regado nunca
boolean restart_falta_wifi; //Flag indica si el dispositivo se reinicio la ultima vez por no poder conectarse al wifi
int errorBME; //Lleva la cuenta de las lecturas erroneas que entrega el sensor y son reportadas con comando V
float sensorsuelomin, sensorsuelomax; //Limites de full wet y dry para el sensor de maceta
int espacioentreriegos; //en minutos
byte ciclosriego=1; //por defecto riega una sola vez.
boolean riegoenespera;
unsigned long timeresperariego;
int sensorsuelominADC, sensorsuelomaxADC; //Las referencias enteras de los niveles del sensor de tierra.
boolean autoriego=1;
unsigned long timerautoriego; //Para llevar la cuenta el momento en que se hizo auto riego, y comprobar que no se haya autoregado hace muy poco asi se evitan errores
unsigned long mindelayautoriego; //Tiempo que tiene que pasar entre un autoriego y otro
byte ciclosautoriego=3; //Esto hay que poder guardarlo en EEPROM!!
int umbralautoriego;
int number_of_resets; //Guarda el numero de veces que se reinicia por no poder conectar. Se guarda en EEPROM y se carga en el inicio
int resetcode; //Codigo de reinicio...si el dispositivo va a reiniciarse por programa se setea y guarda en EEPROM antes, 
//Luego en el inicio se lee este valor y asi se informa si el reseteo fue por programa y no erratico.
  struct id
  {
      String IDTelegram;
      byte alarmflag;
  };
id registroIDs[numclientes]; //array con x estructuras de chat_id y flag de alarma. Para registrar los clientes a quienes responde el bot y a quienes envia las alarmas
float voltajesensorsuelo; //Este va a ser el voltaje del sensor, y datos[2] pasa a ser la humedad en porcentaje
int errorDHTi, errorDHTo;
int averagesuelo=0;

//Recibe una variable con los minutos y lo convierte a texto expresado en minutos, horas o dias segun corresponda
String generartextotiempo(unsigned long minutos) {
  String cadenatemp;
  cadenatemp= "\n";
  if (minutos < 120) {
    cadenatemp+= minutos;
    cadenatemp+=" min";
  } else if (minutos < 1440) {
          cadenatemp+= (int) minutos/60;
          cadenatemp+= " horas";
            if (minutos%60) { 
              cadenatemp+= ",";
              cadenatemp+= minutos%60;
              cadenatemp+=" min";
            }
    } else {
        cadenatemp+=(int) minutos /1440;
          if (minutos>2879) cadenatemp+=" dias";
            else cadenatemp+=" dia";
          if (minutos%1440) { 
            cadenatemp+= ",";
            cadenatemp+= (int) (minutos%1440)/60; //Marca el resto de la division en horas
            cadenatemp+=" horas";
            cadenatemp+= ",";
            cadenatemp+= (int) (minutos%1440)%60;
            cadenatemp+=" min";
          }
      }
  return cadenatemp;
  }

//Envia notificaciones a todos los usuarios registrados, tanto para alarmas como par autoriego
void enviarnotificaciones() {
  while (alarmasporenviar) {
      if (registroIDs[alarmasporenviar-1].IDTelegram!=NULL) { //Empezando por la ultima posicion de Id se fija si no esta vacia y emvia el mensaje de alarma
        bot.sendMessage(registroIDs[alarmasporenviar-1].IDTelegram, cadena, ""); 
        yield();
      }
    alarmasporenviar--;            
  }
}

//Recibe la lectura del ADC y el minimo y maximo del rango efectivo en valores del ADC, devuelve un porcentaje inversamente proporcional a la variacion del valor en ese rango
//Si el dato esta fuera de rango lo limita a 0 o 100
float rango_a_porcentaje_invertido(int dato, int min, int max) {
float temp=100.0-(((dato-min)*1000.0)/((max-min)*10.0)); //modificado para recibir todos valores de ADC enteros
if(dato>max) return 0;
  else if (dato<min) return 100;
    else return temp;
}


/*Es llamada por chequearlarmas. Si el valor de sensor pide riego y ademas pasó tiempo 
 desde el ultimo auto riego inicia el proceso. El mismo continua en el mismo loop por la rutina
 que se encarga del riego manual*/
void iniciarautgoriego() {
 if (millis()-timerautoriego > mindelayautoriego || !firstriego) {     
    Serial.println("Aca detecta condicion de alarma para autoriego");
    Serial.println(millis());
    Serial.print("enciende");
    digitalWrite(valvulariego, true); //Esto en el programa se cambia por D8
    ciclosriego=ciclosautoriego; 
    EEPROM.get(70, tiemporiego); 
    cadena="----Iniciando riego auto----";
    cadena+="\n";    
    cadena += "Sensor: ";   
    cadena += voltajesensorsuelo;
    cadena += "V";
    cadena += " (";
    cadena += (int)datos[2];
    cadena += "%)";
    cadena += "\n";   
    cadena+="\n";
    cadena+="Tiempo de riego: ";
    cadena+=tiemporiego;
    cadena+="min";
    cadena+="\n";
    cadena+="Espacio entre riegos: ";
    cadena+=espacioentreriegos;
    cadena+="min";
    cadena+="\n";
    cadena+="Ciclos de riego: ";
    cadena+=ciclosautoriego;
    cadena+="\n";
    cadena+= " Duracion total: ";
    cadena+= tiemporiego*ciclosautoriego + (espacioentreriegos*(ciclosautoriego-1));  
    cadena+= "min";    
    cadena+= "\n";  
    cadena+="Tiermpo entre autorriegos: ";
    cadena+= "\n";  
    cadena+=mindelayautoriego/60000;
    cadena+="min";
    cadena+="\n";
      if (!riegoencurso) { //Para que solo envie notificaciones una sola vez
        alarmasporenviar=numclientes; //Hace que en el loop se envie una alarma a los clientes como aviso del inicio de riego automatico
        enviarnotificaciones();
      }
    riegoencurso=1;
    timerautoriego=millis(); //Resetea el timer de auto riego, luego en loop al terminar de regar tb lo hace
    timer_riego=millis();
  }
}

//Convierte un entero (solo los primeros 8 bits) en una cadena con si correspondiente binario, para mostrar estado alarma en forma de flags
String convert_int_to_string(int valor) {

String cadena;
  for (int x=0; x<8; x++) {
    if (valor & (1<<x)) cadena +="1";
      else cadena +="0";
  }
cadena += "\n";
return cadena;
}


//Verifica las condiciones de alarma, setea los flags, y tambien llama al autoriego si las condiciones de sensor estan dadas
void chequearalarmas() {
  estadoalarma=0; //resetea la condicion antes de testear
  if ((datos[0] > (float) TempintalarMax) && (maskalarma & B00000010)) bitSet(estadoalarma, 1);
  if ((datos[0] < (float) TempintalarMin ) && (maskalarma & B00000100)) bitSet(estadoalarma, 2);
  if ((voltajesensorsuelo < 0.3) && (maskalarma & B00010000)) bitSet(estadoalarma, 4);
    else if ((voltajesensorsuelo > (((float) sueloalarMin)/100)) && (maskalarma & B00001000)) bitSet(estadoalarma, 3);
      else  if (autoriego && voltajesensorsuelo>float(umbralautoriego)/100) iniciarautgoriego(); //Solo si hay medicion valida del sensor y esta habilitado autoriego y se supera el umbral
  //aca cada vez que se chequean las alarmas y el sensor pide riego se llama a la funcion que inicia autoriego, esta verifica
  //que haya pasado el tiempo suficiente desde la anterior (a menos que sea la primera vez y que autoriego este habilitado para proceder
  if (flagfallaled && (maskalarma & B00100000)) bitSet(estadoalarma, 5);
}

String generartextoalarma(byte estados) {
  String mensajealarma;
    if (estados) mensajealarma="Alarma!!:"; //Solo imprime Alarmas! si hay al menos una de ellas
  mensajealarma += "\n"; 

    if (estados & B00000010) {
      mensajealarma += "-Alta temp: ";
      mensajealarma += datos[0];
      mensajealarma += "C"; 
      mensajealarma += "\n"; 
    }

    if (estados & B00000100) {
      mensajealarma += "-Baja temp: ";
      mensajealarma += datos[0];
      mensajealarma += "C";  
      mensajealarma += "\n"; 
    }

    if (estados & B00001000) {
      mensajealarma += "-Baja hum suelo: ";
      mensajealarma += voltajesensorsuelo;
      mensajealarma += "V"; 
      mensajealarma += "\n";  
    }

    if (estados & B00010000) {
      mensajealarma += "-SE CORTO TOA LA LOOZ!!";
      mensajealarma += "\n"; 
    }

    if (estados & B00100000) {
      mensajealarma += "-FALLA PANEL LED!!";
      mensajealarma += "\n";  
      mensajealarma += generartextotiempo((millis() - timerluz)/60000); 
      mensajealarma += " sin luz";  
    }

    if(!estados) { //si no hay ninguna alarma...
      mensajealarma += "\n"; 
      mensajealarma += "No hay estado de alarmas"; 

    }
  return mensajealarma;
}

void actualizamaxmin() {
    for (byte x=0; x<numparametros; x++) {
      if (datos[x] < datosmin[x]) datosmin[x]=datos[x]; //actualiza minimons
      if (datos[x] > datosmax[x]) datosmax[x]=datos[x]; //actualiza maximos
    }
}

//Lee todos los parametros y los guarda en el array, actualiza max y min chequea condiciones de alarma
void relevardatos() {
  float tempvalue;
  tempvalue=dht.getTemperature();
    if (isnan(tempvalue)) errorDHTi++;
      else datos[0]=tempvalue;
  tempvalue=(float)dht.getHumidity();
    if (isnan(tempvalue)) errorDHTi++;
      else datos[1]=tempvalue;    
  for(byte c=0; c<25;c++) { //toma muchos valores para promediar un poco..
    averagesuelo=(averagesuelo+analogRead(GPIO_NUM_33))/2;
  }
  voltajesensorsuelo=(float)averagesuelo/1024.00 * 3.30;
  datos[2]=(int)rango_a_porcentaje_invertido(averagesuelo, sensorsuelominADC, sensorsuelomaxADC);

  tempvalue=dht2.getTemperature();
    if (isnan(tempvalue)) errorDHTo++;
      else datos[3]=tempvalue;
  tempvalue=(float)dht2.getHumidity();
    if (isnan(tempvalue)) errorDHTo++;
      else datos[4]=tempvalue;
  float temp=bme.readPressure()/100.0F; //para convertir a hectopascales divide por cien
  if (temp>900) datos[5]=temp; //Solo actualiza el valor si es real, sino lo toma como error y deja el anterior
    else errorBME++;    
  datos[6]=1-digitalRead(LUZ_IN); //La salida esta invertida
  actualizamaxmin();
  chequearalarmas();
}


//Para llamar desde setup, toma datos por primera vez y los copia a max y min para empezar a registrar
void inizializamaxmin() {
  relevardatos();
    for (byte x=0; x<numparametros; x++) {    
      datosmin[x]=datos[x];
      datosmax[x]=datos[x];
    }
}

String ipToString(IPAddress ip) {
  String s="";
    for (byte i=0; i<4; i++)
      s += i  ? "." + String(ip[i]) : String(ip[i]); //la primera iteracion solo pone el numero, luego "." mas el numero.
  return s;
}

//consulta cada uno de los ID registrados, si coincide devuelve 1
byte checkID(String Id) { 
  for (byte i=0; i<numclientes; i++) {   
    if (Id == registroIDs[i].IDTelegram) return i+1;
  }
return 0;
}

//Recibe la direccion inicial y la cadena a almacenar en la EEPROM.
void write_word(byte addr, String word2) {
  delay(10);
  byte str_len = word2.length() + 1;
    for (byte i = addr; i < str_len + addr; ++i) {  
      EEPROM.write(i, word2.charAt(i - addr)); // the problem was here
    }
  EEPROM.write(str_len + addr, '\0');
  EEPROM.commit();
}

void registraruser(String texto, String id_de_chat) { //Registra id telegram en la ultima posicion vacia y si estan todas llenas lo guarda en la ultima.
   String respuesta;
   byte n=0;
   Serial.print("txt=");
   Serial.println(texto);

    if (texto == "Thaumat") {     
      while (n<numclientes) {  //escanea el array de chat ids para ver donde guardarlo          
          if (registroIDs[n].IDTelegram==NULL || n==numclientes-1) {
            registroIDs[n].IDTelegram=id_de_chat; // Registra en la posicion n
            write_word(n*12, id_de_chat); //Llama a funcion que registra un string en la eeprom, usando 12 bytes para cada registro.  Se envia el numero de posicion x12 para formar la direccion inicial.
            break;         
          }  
      n++;
      }

      respuesta="chat_id: ";
      respuesta+=id_de_chat;
      respuesta+=" registrado en posicion ";
      respuesta+=String(n);
      Serial.println("respuesta=" + respuesta);
      //Serial.println(respuesta);
      bot.sendMessage(id_de_chat, respuesta, ""); 
    } 
}

//lee todos los chat_id desde la EEPROM y los guarda en el array de estructuras.
void leerdataEEPROM() {
  byte x, pos;
  for (x=0; x<numclientes; x++) {
      String temp;
        for (pos=x*12; pos<(x*12 + 12); pos++ ) {
          if (char(EEPROM.read(pos))=='\0') break;
          temp+=char(EEPROM.read(pos));      
        }
  registroIDs[x].IDTelegram=temp;
  }
}

void showdataeepromraw() { //Para probar, muestra el contenido de la EEPROM para las variable de chat_id
String temp="";
  for (byte y=0; y<48; y++) {
    Serial.print(char(EEPROM.read(y)));
  }
}


void showregistroid() {
  String temp="Contenido inicial EEPROM ";
  temp += "\n";
    for (byte x=0; x<numclientes; x++) {
      temp+="chat_id ";
      temp+=(String)x;
      temp+=": ";
      temp+=registroIDs[x].IDTelegram;
      temp += "\n";
    }
  Serial.print(temp); //imprime el listado del contenido de los chat_ids
}

//Toma como argumento la cadena text recibida por el bot y la analiza para separarla en un comando y un argumento numerico si hubiera. En ese caso convierte el argumento a entero 
//y setea la variable  tieneargumentonum.
String descomponertext(String mensajebot) {
  String cadenatemp;
  boolean flagnum;
  byte index;
  argumentonumerico=0; //si no resetea se va acumulando de mensaje en mensaje
  Serial.print("descomponertext recibe: ");
  Serial.print(mensajebot.length());
  Serial.println(" caracteres.");   
    for (index=0; index<(mensajebot.length()); index++) {  
      if (flagnum) {     
        if (isDigit(mensajebot[index])) {
           argumentonumerico=argumentonumerico*10 + (byte) mensajebot[index]-48 ; //Si es un caracter numerico lo agrega a la variable aplicando el peso
           isvalidnum=1;
           continue; 
        }              
      }              
      if (mensajebot[index]==32) flagnum=1; //32 ascii para space                                 
        else cadenatemp += mensajebot[index];                  
    }
return cadenatemp;
}

//Envia a thinkspeak un dato float en el campo field
void enviardataTS(float data, byte field) {  
   WiFiClient client;
    if (client.connect(server, 80)) { // usar 184.106.153.149 ó api.thingspeak.com
      Serial.println("WiFi Client connected ");    
      String postStr = apiKey; 
        if (field==1) postStr += "&field1=";
          else if (field==2) postStr += "&field2=";
            else if (field==3) postStr += "&field3=";
              else if (field==4) postStr += "&field4=";
                else if (field==5) postStr += "&field5=";
                  else if (field==6) postStr += "&field6=";
                    else if (field==7) postStr += "&field7=";
                      else if (field==8) postStr += "&field8=";
                        else postStr += "&field1="; //Por defecto escribe en campo 1
      postStr += String(data);
      postStr += "\r\n\r\n";  
      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr.length());
      client.print("\n\n");
      client.print(postStr);   
    }
 client.stop();
}

/*Verifica el estado de la conexion, si no esta conectado intenta conectarse
primero con los parametros almacenados y si no puede crea el softAP para configurarlo con el celu.
Si no se entra a configurarlo dentro del timeout (120) la funcion retorna sin hacer nada*/
void GetandCheckConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiManager.setConfigPortalTimeout(60); 
     wifiManager.autoConnect("Green");
     yield();
  }
    if (WiFi.status() != WL_CONNECTED) {
      EEPROM.put(64, number_of_resets);  
      resetcode=1;  //code=1 indica que se reinicio a proposito.    
      EEPROM.put(68, resetcode); 
      EEPROM.commit();
      ESP.restart(); 
    }
}
//Recibe texto enviado al bot y envia mensaje con la respuesta correspondiente
String procesarmsgparam(String texto) {
  String mensajes[]={
    "Maxtemp", //0
    "Mintemp",
    "RestartESP",
    "Alarmasuelo",
    "Umbralautorriego",
    "Tiemposinluz", //5
    "Tiemporriego",
    "Delayautorriego",
    "Ciclosautorriego",
    "Noalarma",
    "Disableautorriego", //10
    "Enableautorriego",
    "Setalarma",
    "Checalarma",
    "Repetiralarma",
    "Disablealta", //15
    "Disablebaja",
    "Disablesuelo",
    "Disablecorte",
    "Disableluz",
    "Q", //20
    "Sensorsuelomin",
    "Sensorsuelomax",
    "Espaciorriego",
    "Startriego",
    "Startriegociclos", //25
    "Stopriego",
    "V",
    "M",
    "Tmpmaxmin",
    "Setsample", //30
    "Save",
    "Blank",
    "Reset",
    "Id",
    "Resetnumresets",
    "Resetclient",
  };

//por alguna razon esto funciona, aunque cada elemento del array sea diferente
byte keywordnumber=sizeof (mensajes) / sizeof (mensajes[0]);

  String respuesta;

    while(keywordnumber) {
        if(texto==mensajes[keywordnumber-1]) break;
      keywordnumber--;
    }

  if(!keywordnumber) {
    respuesta="No te entiendo una garcha..";
    return respuesta;
  }

  keywordnumber--;
  Serial.print("keywordnumber:");
  Serial.println(keywordnumber);

  switch(keywordnumber) { 
    case 0:
          if (isvalidnum) {
            respuesta = "Alarma ALTA temp anterior:";
            respuesta += TempintalarMax;
            TempintalarMax=argumentonumerico;
            respuesta += "\n"; 
            respuesta += "Nueva alarma ALTA temp: ";
            respuesta += TempintalarMax;
            respuesta += "\n";
            respuesta += "Save para guardar permanente";
            respuesta += "\n";        
          }
          break;
    case 1:
        if (isvalidnum) {
          respuesta = "Alarma BAJA temp anterior:";
          respuesta += TempintalarMin;
          TempintalarMin=argumentonumerico;
          respuesta += "\n";
          respuesta += "Nueva alarma BAJA temp: ";
          respuesta += TempintalarMin;
          respuesta += "\n";
          respuesta += "Save para guardar permanente";
          respuesta += "\n";  
        }
        break;  
    case 2:
          respuesta="Reseteando ESP8266..";
          texto="";
          respuesta="";
          numNewMessages = bot.getUpdates(bot.last_message_received + 1); //chequea si bot recibio mensaje
          numNewMessages=0; //Esto puede que no haga falta..pero sino quedaba en un loop de reinicio constante
          bot.sendMessage(chat_id, respuesta, ""); 
          ESP.restart(); 
          break;
    case 3:
          if (isvalidnum) {
            respuesta = "Alarma Hum suelo ant:";
            respuesta += (float) sueloalarMin /100;
            respuesta += "V"; 
            sueloalarMin=argumentonumerico;
            respuesta += "\n";
            respuesta += "Nueva alarma Hum suelo: ";
            respuesta += (float) sueloalarMin /100;
            respuesta += "V";   
            respuesta += "\n";
            respuesta += "Save para guardar permanente";
            respuesta += "\n";  
          }
          break;
    case 4:
          if (isvalidnum) {
            respuesta = "Umbral autoriego ant:";
            respuesta += (float) umbralautoriego/100;
            respuesta += "V";   
            respuesta += "\n";
            umbralautoriego=argumentonumerico;
            respuesta += "Nuevo umbral: ";
            respuesta += (float) umbralautoriego/100;
            respuesta += "V";   
            respuesta += "\n";
            respuesta += " (";
            respuesta += rango_a_porcentaje_invertido(int((umbralautoriego/100.0)*1024/3.3), sensorsuelominADC, sensorsuelomaxADC);
            respuesta += "%)";
            respuesta += "\n";
            respuesta += "Save para guardar permanente";
            respuesta += "\n";  
          }
          break;
    case 5:
          if (isvalidnum) {
            respuesta = "Periodo luz ant:"; 
            respuesta += periodo_luz/60000;
            respuesta += "min";   
            respuesta += "\n";           
            periodo_luz=argumentonumerico*60000;
            respuesta += "Nuevo tiempo maximo sin iluminacion: ";
            respuesta += argumentonumerico;
            respuesta += "min";   
            respuesta += "\n";
            respuesta += "Save para guardar permanente";
            respuesta += "\n";  
            }
          break;
    case 6:
          if (isvalidnum) {
            respuesta = "Tiempo riego ant:";
            respuesta += tiemporiego;
            respuesta += " min";   
            respuesta += "\n";          
            tiemporiego=argumentonumerico;
            respuesta += "Nuevo tiempo de riego: ";
            respuesta += tiemporiego;
            respuesta += " min";   
            respuesta += "\n";
            respuesta += "Save para guardar permanente";
            respuesta += "\n";  
          }
          break;
    case 7:
          if (isvalidnum) {
            respuesta = "Mindelayautoriego ant:";
            respuesta += "\n";
            respuesta += mindelayautoriego/60000;
            respuesta += " min";   
            respuesta += "\n";
            mindelayautoriego=argumentonumerico*60000;
            respuesta += "Tiempo minimo entre riegos auto: ";
            respuesta += "\n";
            respuesta += argumentonumerico;
            respuesta += " min";   
            respuesta += "\n";
          }
          break;
    case 8:
          if (isvalidnum) {
            respuesta = "Ciclos riego auto ant:";
            respuesta += ciclosautoriego;
            respuesta += "\n";
            ciclosautoriego=argumentonumerico;
            respuesta += "New Ciclos riego auto: ";
            respuesta += ciclosautoriego;   
            respuesta += "\n";
            respuesta += "Save para guardar permanente";
            respuesta += "\n";  
          }
          break;
    case 9:
          flagalarma=0;
          respuesta = "Alarmas desactivadas";
          respuesta += "\n";
          respuesta += "Save para guardar permanente";
          respuesta += "\n";  
          break;
    case 10:
          autoriego=0;
          respuesta = "Riego auto desactivado";
          respuesta += "\n";
          respuesta += "Save para guardar permanente";
          respuesta += "\n";  
          break;
    case 11:
          autoriego=1;
          respuesta = "Riego auto activado";
          respuesta += "\n";
          respuesta += "Save para guardar permanente";
          respuesta += "\n";  
          break;
    case 12:
          flagalarma=1;
          maskalarma=255;
          respuesta = "Alarmas activadas:";
          respuesta += "\n";
          respuesta += "-Alarma ALTA temp: ";
          respuesta += TempintalarMax;
          respuesta += "\n";
          respuesta += "-Alarma BAJA temp: ";
          respuesta += TempintalarMin;
          respuesta += "\n";
          respuesta += "-Alarma Hum suelo: ";
          respuesta += (float) sueloalarMin /100;
          respuesta += "V";   
          respuesta += "\n";
          respuesta += "-Corte de looz";   
          respuesta += "\n";
          respuesta += "-Falla iluminacion";   
          respuesta += "\n";
          break;
    case 13:
            if (!maskalarma) respuesta="Ninguna alarma habilitada";
              else respuesta = "Alarmas habilitadas:";
            respuesta += "\n";
            if (maskalarma & B00000010) { 
              respuesta += "-Alta temp";
              respuesta += "\n";   
            } 
            if (maskalarma & B00000100) { 
              respuesta += "-Baja temp";
              respuesta += "\n";   
            } 
            if (maskalarma & B00001000) { 
              respuesta += "-Hum suelo";
              respuesta += "\n";   
            } 
            if (maskalarma & B00010000) { 
              respuesta += "-Corte de luz";
              respuesta += "\n";   
            } 
            if (maskalarma & B00100000) { 
              respuesta += "-Falla luz grow";
              respuesta += "\n"; 
              respuesta += "\n";       
            } 
            relevardatos();
            respuesta += generartextoalarma(estadoalarma);
            respuesta += "\n";
            break;
    case 14:
            if (isvalidnum) {
              respuesta = "Repe alarmas ant: ";
              respuesta += tiempochequeoalarma/60000;
              respuesta += "min";   
              respuesta += "\n";
              tiempochequeoalarma=argumentonumerico*60000; //en milisegundos
              respuesta += "Repeticion alarma: ";
              respuesta += argumentonumerico;
              respuesta += "min";   
              respuesta += "\n";
              respuesta += "Save para guardar permanente";
              respuesta += "\n";  
            }
            break;
    case 15:
            respuesta = "Alarma por alta temperatura inhabilitada";
            bitClear(maskalarma, 1);
            break;
    case 16:
            respuesta = "Alarma por baja temperatura inhabilitada";
            bitClear(maskalarma, 2);
            break;
    case 17:
            respuesta = "Alarma por sensor de suelo inhabilitada";
            bitClear(maskalarma, 3);
            break;
    case 18:
            respuesta = "Alarma por corte de luz inhabilitada";
            bitClear(maskalarma, 4);  
            break;
    case 19:
            respuesta = "Alarma por falta de luz cultivo inhablilitada";
            bitClear(maskalarma, 5);  
            break;
    case 20:
            relevardatos();
            chequearalarmas();
            respuesta = "-Tint=";
            respuesta += String(datos[0],1);
            respuesta += "C";
            respuesta += " ; Hint=";
            respuesta += String(datos[1],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "-Tierra= ";
            respuesta += String(datos[2],0);
            respuesta += "%";
            respuesta += " (";
            respuesta += voltajesensorsuelo;
            respuesta += "V)";
            respuesta += "\n";
            respuesta += "(AveragesensorADC=";
            respuesta += averagesuelo;
            respuesta += ")";
            respuesta += "\n";
            respuesta += "-Text=";
            respuesta += String(datos[3],1);
            respuesta += "C";
            respuesta += " ; Hext=";
            respuesta += String(datos[4],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "-Press=";
            respuesta += String(datos[5],1);
            respuesta += " hPa";
            respuesta += "\n";
            respuesta += "-Luz=";
            if (datos[6]) respuesta += " Encendida";
              else respuesta+= " Apagada";  
            respuesta += "\n";  
              if (firstriego && !riegoencurso) { 
                respuesta +="----Ultimo riego hace:---- ";
                respuesta +=generartextotiempo((millis()-marcador_riego)/60000);
                respuesta += "\n";  
                  if (autoriego && !riegoencurso) {
                    if (millis()-timerautoriego < mindelayautoriego) {
                      respuesta+="-Faltan: ";
                      respuesta+=(mindelayautoriego-(millis()-timerautoriego))/60000;
                      respuesta+="min para habilitar ";
                      respuesta += "proximo autorriego";
                    } else respuesta += "autorriego activado";
                
                  } else respuesta+= "autorriego desactivado";              
              } 
            if (estadoalarma) {                      
              respuesta += "-Alarmas=";     
              respuesta += convert_int_to_string(estadoalarma);   
              respuesta += "\n";
            } else respuesta+="-No hay alarmas.." ;
            respuesta += "\n";
            if (riegoencurso) respuesta+="Riego en curso!!!";
            break;
    case 21:
            if (isvalidnum) {
              respuesta = "Hum suelo MIN ant:";
              respuesta += sensorsuelomin;
              respuesta += "V-("; 
              respuesta += sensorsuelominADC;
              respuesta += ")";
              respuesta += "\n";              
              sensorsuelomin=(float) argumentonumerico/100;
              sensorsuelominADC=(int) (sensorsuelomin*1024)/3.3;
              respuesta +="Nuevo nivel maxima humedad suelo:";
              respuesta += "\n";  
              respuesta += sensorsuelomin;
              respuesta += "V"; 
              respuesta += "\n";
              respuesta += "Save para guardar permanente";
              respuesta += "\n";     
            }
            break;
    case 22:
            if (isvalidnum) {
                respuesta = "Hum suelo MAX ant:";
                respuesta += sensorsuelomax;
                respuesta += "V-("; 
                respuesta += sensorsuelomaxADC;
                respuesta += ")";
                respuesta += "\n";                 
                sensorsuelomax=(float) argumentonumerico/100;
                sensorsuelomaxADC=(int) (sensorsuelomax*1024)/3.3;
                respuesta +="Nuevo nivel max sequedad suelo:";
                respuesta += "\n";  
                respuesta += sensorsuelomax;
                respuesta += "V"; 
                respuesta += "\n";
                respuesta += "Save para guardar permanente";
                respuesta += "\n";     
            }
            break;
    case 23:
            if (isvalidnum) {
                respuesta = "T entre ciclos ant:";
                respuesta += espacioentreriegos;
                respuesta += " min"; 
                respuesta += "\n";  
                espacioentreriegos=argumentonumerico;
                respuesta +="Nuevo tiempo entre ciclos riego:";
                respuesta += "\n";  
                respuesta += espacioentreriegos;
                respuesta += " min"; 
                respuesta += "\n";   
            }
            break;
    case 24:
            if (riegoencurso) {
              respuesta = "Riego actualmente en curso..";
              respuesta += "\n";    
              respuesta += "Stopriego para finalizar";  
              respuesta += "\n";                                  
            } else {  if (isvalidnum) tiemporiego=argumentonumerico; //aca modifico para poder cambiar el tiempo de riego
                        else EEPROM.get(70, tiemporiego); //Si no hay argumento carga por defecto tiemporiego de la EEPROM
                      digitalWrite(valvulariego, true);
                      Serial.print("Enciende manual: ");
                      Serial.println(millis());
                      riegoencurso=1;
                      ciclosriego=1;
                      riegoenespera=0;
                      timer_riego=millis();
                      respuesta = "Comenzando riego...";    
                      respuesta += "\n";  
                      respuesta += "Tiempo riego: ";
                      respuesta += tiemporiego;
                      respuesta += "min";  
                      respuesta += "\n";
                        if (firstriego) { // Solo muestra cuando ya se rego una vez
                          respuesta += " Ultimo riego hace: ";                     
                          respuesta += generartextotiempo((millis()-marcador_riego)/60000);                           
                        }                                       
                    }
            break;
    case 25:
            if (riegoencurso) {
              respuesta = "Riego actualmente en curso..";
              respuesta += "\n";    
              respuesta += "Stopriego para finalizar";  
              respuesta += "\n";                                  
            } else {  if (isvalidnum) ciclosriego=argumentonumerico; //aca modifico para poder cambiar el tiempo de riego
                        else ciclosriego=1; //Si no hay argumento carga por defecto tiemporiego de la EEPROM
                      digitalWrite(valvulariego, true);
                      Serial.print("Enciende manual: ");
                      Serial.println(millis());
                      riegoencurso=1;
                      riegoenespera=0;
                      timer_riego=millis();
                      EEPROM.get(70, tiemporiego); //carga tiempo riego por defecto.
                      respuesta = "Comenzando riego...";
                      respuesta += "\n";
                      respuesta += " Duracion total: ";
                      respuesta += tiemporiego*ciclosriego + (espacioentreriegos*(ciclosriego-1));  
                      respuesta += "min";    
                      respuesta += "\n";  
                      respuesta += "Tiempo riego: ";
                      respuesta += tiemporiego;
                      respuesta += "min";  
                      respuesta += "\n";
                        if (ciclosriego > 1) {                    
                          respuesta += "Tiempo entre ciclos: "; 
                          respuesta += espacioentreriegos;  
                          respuesta += "min";  
                          respuesta += "\n";
                        }
                        if (firstriego) { // Solo muestra cuando ya se rego una vez
                          respuesta += " Ultimo riego hace: ";                     
                          respuesta += generartextotiempo((millis()-marcador_riego)/60000);                           
                        }                                       
                    }
            break;
    case 26:
            if (!riegoencurso) {
              respuesta = "No hay riego en curso..";
              respuesta += "\n";    
              respuesta += "Startriego para iniciar";  
              respuesta += "\n";                                  
            }  else {
                      digitalWrite(valvulariego, false);
                      Serial.print("Apaga manual: ");
                      Serial.println(millis());
                      riegoencurso=0;
                      riegoenespera=0;
                      firstriego=1;
                      marcador_riego=millis();
                      respuesta = "Riego finalizado...";
                      respuesta += "\n";
                    }
            break;
    case 27:
            respuesta="**Version de software: ";
            respuesta +=(String) version;
            respuesta +=" Martin Laskievich";
            respuesta += "\n";
            respuesta += "\n";
            respuesta +="-Wifi Status: ";
            respuesta +=(String) WiFi.status();
            respuesta += "\n";
            respuesta +="-SSID: ";
            respuesta +=(String) WiFi.SSID();
            respuesta += "\n";
            respuesta +="Wifi channel: ";
            respuesta +=(String) WiFi.channel();
            respuesta += "\n";
            respuesta +="IP: ";
            respuesta +=ipToString(WiFi.localIP());
            respuesta += "\n";
            respuesta += "\n";
            respuesta +="-Entre muestras: ";
            respuesta +=(String) (tiempoentremuestras/60000);
            respuesta +="min";
            respuesta += "\n";
            respuesta +="-Max sin LEDs: ";
            respuesta +=(String) (periodo_luz/60000);
            respuesta +="min";
            respuesta += "\n"; 
            respuesta += "\n";
            if (flagalarma) respuesta +="***Alarmas activas***";
              else respuesta +="**Alarmas desactivadas**";
            respuesta += "\n";        
            respuesta +="-Repeticion alarmas: ";
            respuesta +=(String) (tiempochequeoalarma/60000);
            respuesta +="min";
            respuesta += "\n";
            respuesta += "\n";
            respuesta +="-Tiempo de riego: ";
            EEPROM.get(70, tiemporiego);
            respuesta +=tiemporiego;
            respuesta +="min";
            respuesta += "\n";   
            respuesta +="-Tiempo entre ciclos: ";
            respuesta +=espacioentreriegos;
            respuesta +="min";
            respuesta += "\n";  
            respuesta +="-Ciclos en riego auto: ";
            respuesta +=ciclosautoriego;
            respuesta += "\n"; 
            respuesta +="-Tiempo min entre riegos auto: "; 
            respuesta +=mindelayautoriego/60000;
            respuesta +="min";
            respuesta += "\n";
            respuesta += "\n";
            respuesta +="*Sensor suelo min: ";
            respuesta +=sensorsuelomin;
            respuesta +="V";
            respuesta += "\n"; 
            respuesta +="*Sensor suelo max: ";
            respuesta +=sensorsuelomax;
            respuesta +="V";
            respuesta += "\n\n";
              if (firstriego) { 
                respuesta +="---Ultimo riego hace:---";
                respuesta +=generartextotiempo((millis()-marcador_riego)/60000);  
                respuesta += "\n";
              }          
            respuesta +="*Alarma ALTA temp: ";
            respuesta +=(String) TempintalarMax;
            respuesta +=" C";
            respuesta += "\n";
            respuesta +="*Alarma BAJA temp: ";
            respuesta +=(String) TempintalarMin;
            respuesta +=" C";
            respuesta += "\n";
            respuesta +="*Alarma sens suelo: ";
            respuesta +=(float) sueloalarMin /100;
            respuesta +=" V";
            respuesta += "\n";
            if (autoriego) {  
              respuesta +="---Umbral riego auto:---";
              respuesta += "\n";
              respuesta +=(float) umbralautoriego /100;
              respuesta +=" V  ";
              respuesta += " (";
              respuesta += rango_a_porcentaje_invertido(int((umbralautoriego /100.00)*1024/3.3), sensorsuelominADC, sensorsuelomaxADC);
              respuesta += "%)";
              respuesta += "\n";
            } else {
                respuesta += "-Autorriego apagado";
                respuesta += "\n";
              }
            respuesta += "\n";
            respuesta +="-Periodo MaxMin: ";
            respuesta +=(String) (periodomaxmin);
            respuesta +="min";
            respuesta += "\n";
            respuesta += "\n";
            respuesta +="-----Tiempo activo:----";
            respuesta +=generartextotiempo(millis()/60000);
            respuesta += "\n"; 
            respuesta += "\n";      
            for(byte y=0; y < numclientes; y++) {
              respuesta +="-Cliente ";
              respuesta +=(String) y;
              respuesta +=":  ";
              respuesta +=registroIDs[y].IDTelegram; 
              respuesta += "\n"; 

            }
            respuesta +="*Errores BME: ";
            respuesta +=errorBME;
            respuesta += "\n";
            respuesta +="*Errores DHText: ";
            respuesta +=errorDHTo;
            respuesta += "\n";
            respuesta +="*Errores DHTint: ";
            respuesta +=errorDHTi;
            respuesta += "\n";
            break;
    case 28:
            respuesta = "Max/Min hace: ";
            respuesta += (String) ((millis()-timermaxmin)/60000);
            respuesta += " min";
            respuesta += "\n";
            respuesta += "TintMAX=";
            respuesta += String(datosmax[0],1);
            respuesta += "C";
            respuesta += "\n";
            respuesta += "TintMIN=";
            respuesta += String(datosmin[0],1);
            respuesta += "C";
            respuesta += "\n";
            respuesta += "Delta=";
            respuesta += (String) (datosmax[0]-datosmin[0]);
            respuesta += "C";            
            respuesta += "\n";
            respuesta += "HintMAX=";
            respuesta += String(datosmax[1],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "HintMIN=";
            respuesta += String(datosmin[1],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "Delta=";
            respuesta += (String) (datosmax[1]-datosmin[1]);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "\n";
            respuesta += "TExtMAX=";
            respuesta += String(datosmax[3],1);
            respuesta += "C";
            respuesta += "\n";
            respuesta += "TExtMIN=";
            respuesta += String(datosmin[3],1);
            respuesta += "C";
            respuesta += "\n";
            respuesta += "Delta=";
            respuesta += (String)(datosmax[3]-datosmin[3]);
            respuesta += "C";
            respuesta += "\n";
            respuesta += "HExtMAX=";
            respuesta += String(datosmax[4],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "HExtMIN=";
            respuesta += String(datosmin[4],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "Delta=";
            respuesta += (String)(datosmax[4]-datosmin[4]);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "\n";
            respuesta += "SueloMax=";
            respuesta += String(datosmax[2],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "SueloMIN=";
            respuesta += String(datosmin[2],0);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "Delta=";
            respuesta += (String)(datosmax[2]-datosmin[2]);
            respuesta += "%";
            respuesta += "\n";
            respuesta += "\n";
            respuesta += "PSmax=";
            respuesta += String(datosmax[5],1);
            respuesta += "hPa";
            respuesta += "\n";
            respuesta += "PSMin=";
            respuesta += String(datosmin[5],1);
            respuesta += "hPa";
            respuesta += "\n";
            respuesta += "Delta=";
            respuesta += (String)(datosmax[5]-datosmin[5]);
            respuesta += "\n";
              if (datosmax[6]==datosmin[6]) { 
                respuesta += "Luz Sin cambios. ";
                respuesta += "\n";
              }
                else respuesta+="Hubo cambio. ";
              if (datos[6]) respuesta+=" Ahora encendida";
                else respuesta+=" Ahora apagada";
            respuesta += "\n";  
            break;
    case 29:
            if (isvalidnum) {
            respuesta = "Periodo maxmin ant: ";
            respuesta += periodomaxmin;
            respuesta += "min";
            respuesta += "\n";            
            periodomaxmin=argumentonumerico;
            respuesta += "Nuevo periodo MaxMin: ";
            respuesta +=(String) periodomaxmin;
            respuesta += "min";
            respuesta += "\n";
            }       
            break;
    case 30:
            if (isvalidnum) {
            respuesta = "T e/muestras ant: ";
            respuesta += tiempoentremuestras/60000;
            respuesta += "min";
            respuesta += "\n";            
              if (argumentonumerico<2) tiempoentremuestras=2*60000; //Si el tiempo es menor a 2 minutos lo pone en 2. Sino no llega a enviar todo a thingspeak y se pisa
                else tiempoentremuestras=argumentonumerico*60000;
            respuesta += "Nuevo periodo muestras: ";
            respuesta +=(String) argumentonumerico;
            respuesta += "min";
            respuesta += "\n";
            }    
            break;
    case 31:
            //Graba los parametros en la EEPROM
            Serial.print("Guarda parametros permanentemente");
            EEPROM.put(50, periodomaxmin); 
            EEPROM.put(52, flagalarma); 
            EEPROM.put(54, tiempoentremuestras/60000);
            EEPROM.put(56, autoriego); 
            EEPROM.put(58, TempintalarMax);
            EEPROM.put(60, TempintalarMin);
            EEPROM.put(62, sueloalarMin);
            EEPROM.put(64, number_of_resets);
            EEPROM.put(66, tiempochequeoalarma/60000);
            EEPROM.put(68, resetcode);
            EEPROM.put(70, tiemporiego);  
            EEPROM.put(74, periodo_luz/60000); 
            EEPROM.put(78, espacioentreriegos);  
            EEPROM.put(82, sensorsuelominADC);  
            EEPROM.put(86, sensorsuelomaxADC); 
            EEPROM.put(90, ciclosautoriego);     
            EEPROM.put(94, mindelayautoriego/60000);  
            EEPROM.put(98, umbralautoriego);                
            EEPROM.commit(); 
            respuesta="Parametros actualizados en la EEPROM"; 
            break;
    case 32:
            //Limpia la EEPROM. Borra los chat id registrados y carga parametros por default   
            Serial.print("graba ceros en eeprom 00 a 47");
              for (byte index=0; index<48; index++) {
                  EEPROM.write(index, 0);    
              }
            EEPROM.put(50, 240); //Guarda parametros default
            EEPROM.put(54, 5);
            EEPROM.put(58, 35);
            EEPROM.put(60, 23);
            EEPROM.put(62, 200);
            EEPROM.put(66, 10);
            EEPROM.put(70, 1);  
            EEPROM.put(74, 12);   
            EEPROM.put(78, 10);  
            EEPROM.put(82, 270); 
            EEPROM.put(86, 470); 
            EEPROM.put(90, 3);  
            EEPROM.put(94, 120); 
            EEPROM.put(98, 130);                            
            EEPROM.commit();
            respuesta="chat_ids borrados en la EEProm y parametros a default";  
    case 33:
            respuesta="Maximos y minimos reseteados!";
            inizializamaxmin();
            timermaxmin=millis();  
            break;
    case 34:
            respuesta="Ids de chat registrados ";
            respuesta += "\n";
              for (byte x=0; x<numclientes; x++) {
                respuesta+="chat_id ";
                respuesta+=(String)x;
                respuesta+=": ";
                respuesta+=registroIDs[x].IDTelegram;
                respuesta += "\n";            
              }
            break;
    case 35:
            respuesta="Contador reinicios reseteado";
            respuesta += "\n";
            EEPROM.put(64, 0);
            number_of_resets=0;
            EEPROM.commit();
            break;
    case 36:
            respuesta="Cliente borrado";
            respuesta += "\n";
            for (byte x=(clienteactual-1)*12; x<((clienteactual-1)*12+12); x++) {
                EEPROM.write(x, 0);    
            }
            EEPROM.commit(); 
            leerdataEEPROM(); //Vuelve a cargar los registros de id actualizados
            break;
    
    default:
            break;
  }

  return respuesta;
}


void setup() {
  Serial.begin(115200);
	// Initialize temperature sensor 1
	dht.setup(DHT_INT_PIN, DHTesp::DHT22);
	// Initialize temperature sensor 2
	dht2.setup(DHT_EXT_PIN, DHTesp::DHT22);


bme.begin(0x76);


  pinMode(LED_BUILTIN, OUTPUT);
  if (!prueba) pinMode(LUZ_IN, INPUT); //para que si esta en prueba no habilite el pin D7 (Entrada digital del sensor de luz), que va conectado al led para simular la valvula de riego 
  pinMode(valvulariego, OUTPUT);
  GetandCheckConnection();  
    if (WiFi.status() == WL_CONNECTED) {   
      Serial.println("Conectado a: " + WiFi.SSID());
     secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
      Serial.println("Retrieving time: ");
      configTime(0, 0, "pool.ntp.org"); // Probando ahora con servidor de Argentina!!
      time_t now = time(nullptr);
        while (now < 24 * 3600) {       
          Serial.print(".");
          delay(100);
          now = time(nullptr);
        } 
      Serial.println(now); 
      timermaxmin=millis();
    }
  EEPROM.begin(128); // Set aside some memory
  leerdataEEPROM();
  showregistroid();
  //carga desde la EEPROMtodos los parametros
  EEPROM.get(50, periodomaxmin); 
  if (periodomaxmin>1440) periodomaxmin=1440;
  EEPROM.get(52, flagalarma); 
  EEPROM.get(54, tiempoentremuestras); 
  EEPROM.get(56, autoriego); 
  EEPROM.get(58, TempintalarMax);
  EEPROM.get(60, TempintalarMin);
  EEPROM.get(62, sueloalarMin);
  if (sueloalarMin>400) sueloalarMin=400;
  EEPROM.get(64, number_of_resets); 
  EEPROM.get(66, tiempochequeoalarma); 
  if (tiempochequeoalarma>30) tiempochequeoalarma=30;
  EEPROM.get(68, resetcode);
  EEPROM.get(70, tiemporiego);
  EEPROM.get(74, periodo_luz);
  EEPROM.get(78, espacioentreriegos); 
  EEPROM.get(82, sensorsuelominADC); 
  EEPROM.get(86, sensorsuelomaxADC); 
  EEPROM.get(90, ciclosautoriego); 
  EEPROM.get(94, mindelayautoriego);
  EEPROM.get(98, umbralautoriego);
  EEPROM.put(68, 0); //Pone a cero resetcode en la EEPROM
  EEPROM.commit();
  sensorsuelomin=float(sensorsuelominADC)/1024*3.3;
  sensorsuelomax=float(sensorsuelomaxADC)/1024*3.3;
  tiempochequeoalarma*=60000; //pasa a milisegundos
  tiempoentremuestras*=60000;
  mindelayautoriego*=60000;
  periodo_luz=periodo_luz*60000;
  tiempoultimamuestra=millis();
  cadena="Dispositivo reiniciado por";
  cadena += "\n"; 
  if (resetcode) cadena += "Problema de conexion";
    else cadena += "Hardware";
  cadena += "\n";
  if (resetcode) {
    cadena += "Resets NO conexion: ";
    cadena += number_of_resets;   
  }
    for (byte i=0; i<numclientes; i++) { //Envia mensaje de reinicio a todos los usuarios regitrados
          if (registroIDs[i].IDTelegram!=NULL) bot.sendMessage(registroIDs[i].IDTelegram, cadena, ""); 
    }    
  relevardatos();
  inizializamaxmin(); //releva datos y pone los arrays de max y minimo a los mismos valores  
}

 

void loop() {

  if (millis() - bot_lasttime > BOT_MTBS) {                        
    numNewMessages = bot.getUpdates(bot.last_message_received + 1); //chequea si bot recibio mensaje
    
      while (numNewMessages) {     //Aca cambie if por while para ver si no pierde mensajes, esto lo hace en los ejemplos de la libreria
        chat_id=bot.messages[0].chat_id; //Responde al remitente
        isvalidnum=0; //resetea el flag de argumento numerico cada vez que recibe un nuevo mensaje
        text=descomponertext(bot.messages[0].text); //mensaje recibido en text
          if (clienteactual=checkID(chat_id)) { 
            bot.sendMessage(chat_id, procesarmsgparam(text) , ""); 
            Serial.print("Mensaje: ");
            Serial.print(text);
            Serial.print(" de cliente num=");
            Serial.println(clienteactual);
          }  
            else registraruser(text, chat_id); 
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        bot_lasttime = millis();    
      }
 
      laststateluz=stateluz;  //En el bucle que releva el bot y se ejecuta periodicamente actualiza los estados de la luz.
      stateluz=1-digitalRead(LUZ_IN); 
      ////////////////////////////////Aca se chequea los estados y el timer para saber si se verifica una falla de los paneles de iluminacion    
        if (!laststateluz) {

            if (stateluz) { 
            //Serial.println("Resetea alarma");
              flagfallaled=0;
            } else if ((millis() - timerluz > periodo_luz) && !flagfallaled) {   
                      flagfallaled=1; 
                //    Serial.println("Setea alarma"); 
                    }
        } else if (!stateluz) {  
                  timerluz=millis();
             //   Serial.println("Set timer"); 
                }


        if (millis() - ultimochequeoalarma > tiempochequeoalarma) { //Aca entra cada vez que verifica alarmas, varios minutos...
          ultimochequeoalarma=millis();
          relevardatos(); //Actualiza los estados de alarmas para no quedar colgado con info vieja
          cadena=generartextoalarma(estadoalarma);
          yield();
            if (estadoalarma && flagalarma) alarmasporenviar=numclientes;
              else alarmasporenviar=0;        
            if (WiFi.status() != WL_CONNECTED) { 
              Serial.println("Reiniciando por falta de GUIFI..."); 
              number_of_resets++; //Incrementa para luego mostrar en el inicio.
              EEPROM.put(64, number_of_resets);  
              resetcode=1;  //code=1 indica que se reinicio a proposito.    
              EEPROM.put(68, resetcode); 
              EEPROM.commit();
              ESP.restart(); //Para probar que si no hay conexion cuando verifica las alarmas reinicia el ESP
            }
        }

        if (alarmasporenviar) enviarnotificaciones(); 

        if (riegoencurso) {    //Aca solo entra cuando esta regando manual o auto
            if (riegoenespera) {          
              if (millis() - timeresperariego > espacioentreriegos*60000) {
                digitalWrite(valvulariego, true);
                Serial.print("Enciende: ");
                Serial.println(millis());
                timer_riego=millis(); 
                riegoenespera=0; 
              } 
            }
              else if (ciclosriego) {
                if (millis() - timer_riego > tiemporiego*60000) {                 
                    digitalWrite(valvulariego, false);
                    Serial.print("Apaga: ");
                    Serial.println(millis());
                    timeresperariego=millis();
                    marcador_riego=millis();         
                    ciclosriego--;       
                    if (ciclosriego) riegoenespera=1;
                      else { 
                        riegoencurso=0; 
                        bitClear(estadoalarma, 3); 
                        timerautoriego=millis(); 
                        Serial.println("Aca termina ciclo de riego"); 
                        Serial.println(millis()); 
                      } 
                      //Aca resetea el estado de autoriego.
                      //..e inicia el contador para espaciar los riegos automaticos. Tb demoraria el autoriego si se ejecuta un riego manual 
                    firstriego=1;  
                }
              }         
        }  

        if (flagled) { //Aca siempre hace parpadear el led..
          digitalWrite(LED_BUILTIN, false); //Enciende y apaga led onboard para mostrar funcionamiento
          flagled=0; 
        } else {  
            digitalWrite(LED_BUILTIN, true);
            flagled=1; 
          }
  } 
 //Serial.println("Loop..");
  if (millis() - tiempoultimamuestra > tiempoentremuestras) { //Aca solo entra cada varios minutos para subir a thingspeak...
    //Para generar un delay de 15 segundos entre subir una muestra y otra, eso es limitacion de thingspeak
    if (!thingspeakwaitflag) {
      if (((millis()-timermaxmin)/60000)>periodomaxmin) {
        timermaxmin=millis(); // Si el timer de max y min llego al tiempo especificado (por ejemplo 12 horas) se resetea.
        yield();
        inizializamaxmin();
      }
      relevardatos();
      GetandCheckConnection(); //Checa la conexion cada vez que se tome una nueva muetra
        if (WiFi.status()!=WL_CONNECTED) {
        EEPROM.put(64, number_of_resets);  
        resetcode=1;  //code=1 indica que se reinicio a proposito.    
        EEPROM.put(68, resetcode); 
        EEPROM.commit();
        ESP.restart();
        }
      yield();
      enviardataTS(datos[indice],1); //Envia primer dato del array
      Serial.println("Data 0 sent to Thingspeak");
      thingspeakwaitflag=1;
      thingspeakwaitTimer=millis();
      indice++;

    } else if ((millis()-thingspeakwaitTimer)>delaythingspeak) {                  
              if (indice<numparametros) {
                  GetandCheckConnection();
                  yield();
                  enviardataTS(datos[indice],indice+1);
                  Serial.print("Data ");
                  Serial.print(indice);
                  Serial.println(" sent to thingspeak");
                  thingspeakwaitTimer=millis();
                  indice++;
              } else {
                  indice=0;
                  thingspeakwaitflag=0;
                  tiempoultimamuestra=millis();
                }
          }                   
  }
}
