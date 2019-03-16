/*
Projeto para controle da porta de sala
v 1.1.2
Software:
  Danielly
  Patricio Oliveira
  Ricardo Cavalcanti
  Mohamad Sadeque
Hardware:
  Wesley Wagner
  Mohamad Sadeque
*/

#include <FS.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <SaIoTDeviceLib.h>

#define CHAVE 14
#define APMODE 13

//Constantes Adicionadas
bool statesaved;//Pode incluir statesaved1, statesaved2, statesaved3... E salvar onde (função) o estado muda.
const String _path = "/state.txt";
// const String _patht = "/statetest.txt"; //Testando criação de arquivo

//Funções Adicionadas
void openFS(void);
void chartobool(const String &content, bool &_state);   //Retorna flag _state bool TRUE se o char salva for '1'. E FALSE C.C.
void writeFile(const bool &_state, const String _path); 
bool readFile(const String _path);                      //Retorna Estado Salvo 
//================================================

volatile bool stateLED = true;
const unsigned short int LED = 10;
unsigned long int lastTime = 0;
unsigned long int delayLeitura = 0;
bool ultimoEstado;
short int media = 0;
short int leituras = 0;
const unsigned short int RELE = 13;
bool lendo = false;
void lightOn();
void lightOff();
void report(bool);
void interrupcao();
void calcMedia();
//Parametros da conexão
WiFiClient espClient;

//Parametros do device
SaIoTDeviceLib sonoff("VEC", "VEC", "wwesley@email.com");
SaIoTController onOff("{\"key\":\"on\",\"class\":\"onoff\",\"tag\":\"ON\"}");
String senha = "12345678910";

//Variveis controladores
volatile bool reconfigura = false;

//Funções controladores
void interruptor();
void setReconfigura();
void setOn(String);
//Funções MQTT
void callback(char *topic, byte *payload, unsigned int length);
//Funções padão
void setup(void);
void loop(void);
//funções
void setupOTA(void);
//================================================
void setup(void)
{
  Serial.begin(115200);
  //Serial.println("------------setup----------");
  // pinMode(RECONFIGURAPIN, INPUT_PULLUP);
  openFS();//Abre o sistema de Arquivos
  statesaved = readFile(_path); //Executa ultimo estado salvo na flash
  if (statesaved)
  {
    digitalWrite(RELE, HIGH);
    Serial.println("SETUP - Last State Saved: OFF");
  }
  else
  {
    digitalWrite(RELE, LOW);
    Serial.println("SETUP - Last State Saved: ON");
  };
  // SPIFFS.remove(_patht);//Remove o arquivo de teste.

  pinMode(CHAVE, INPUT_PULLUP);
  pinMode(APMODE, INPUT_PULLUP);
  pinMode(RELE, OUTPUT);
  delay(80);
  attachInterrupt(digitalPinToInterrupt(CHAVE), interrupcao, CHANGE);
  sonoff.addController(onOff);
  sonoff.preSetCom(espClient, callback, 60);
  sonoff.start(senha);
  ultimoEstado = digitalRead(CHAVE);
  setupOTA();
}

void loop()
{
  //Serial.print("leitura butao: ");
  //Serial.println(digitalRead(BUTTON));
  if (!digitalRead(APMODE))
  {
    Serial.println("Modo AP");
    Serial.print("Last State Saved:");
    if (statesaved)
    {
      Serial.print("Switch - Off");
    }else
    {
      Serial.print("Switch - On");
    };
    delay(10000);
    WiFi.disconnect();
    delay(1000);
    ESP.restart();
  };

  int tentativa = 0;
  sonoff.handleLoop();
  if (lendo)
  {
    calcMedia();
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(500);
    if (++tentativa >= 5)
    {
      ESP.restart();
    }
  }
  Serial.flush();

  stateLED ? lightOn() : lightOff();
  // Serial.println("PORTAA0)");
  // Serial.println(analogRead(A0));
}

void openFS(void)
{
  //Abre o sistema de arquivos
  if (!SPIFFS.begin())
  {
    Serial.println("\nErro ao abrir o Sistema de Arquivos FLASH");
  }
  else
  {
    Serial.println("\nSistema de Arquivos FLASH aberto");
  }
}

void writeFile(const bool &_state, const String _path)
{
  File file_state = SPIFFS.open(_path, "w+");
  if (!file_state)
  {
    Serial.println("Falha ao abrir arquivo");
    return;
  }
  if (_state)
  {
    file_state.print('1');
    Serial.print("Estado Salvo na Flash: OFF (1)");
  }
  else
  {
    file_state.print('0');
    Serial.print("Estado Salvo na Flash: ON (0)");
  };
  Serial.print("path: ");
  Serial.print(_path);
  Serial.println();
  file_state.close();
  return;
}

bool readFile(const String _path) //Retorna Estado Salvo
{
  bool prov_state;
  if(SPIFFS.exists(_path))
  {
  File file_state = SPIFFS.open(_path, "r");
  if (!file_state)
  {
    Serial.println("Erro ao abrir arquivo");
    prov_state = false;
    return prov_state;
  }
  String content = file_state.readStringUntil('\r');
  chartobool(content, prov_state);
  Serial.print("leitura de estado salva: ");
  file_state.close();
  return prov_state;
  }else
  {
    Serial.print("Criando arquivo...");
    writeFile(false, _path);//Estado Nativo ON
    return false;
  }
}

void chartobool(const String &content, bool &_state) //Retorna Estado true se content for '1'. false C.C.
{
  if (content[0] == '1')
  {
    _state = true;
    return;
  }
  _state = false;
  return;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String payloadS;
  Serial.print("Topic: ");
  Serial.println(topic);
  for (unsigned int i = 0; i < length; i++)
  {
    payloadS += (char)payload[i];
  }
  if (strcmp(topic, sonoff.getSerial().c_str()) == 0)
  {
    Serial.println("SerialLog: " + payloadS);
  }
  if (strcmp(topic, (sonoff.getSerial() + onOff.getKey()).c_str()) == 0)
  {
    Serial.println("Value: " + payloadS);
    setOn(payloadS);
  }
}

void setReconfigura()
{
  reconfigura = true;
}

void setOn(String json)
{
  if (json == "1")
  {
    stateLED = true;
  }
  else
  {
    stateLED = false;
  }
}

void setupOTA()
{

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void lightOn()
{
  digitalWrite(RELE, LOW);
  if(statesaved)
  {
  writeFile(false, _path); //Salva apenas estados modificados
  statesaved = false;
  };
}
void lightOff()
{
  digitalWrite(RELE, HIGH);
  if(!statesaved)
  {
  writeFile(true, _path); //Salva apenas estados modificados
  statesaved = true;
  };
}

void calcMedia()
{
  if (abs(millis() - delayLeitura) > 25)
  {
    if (leituras < 20)
    {
      if (ultimoEstado ^ digitalRead(CHAVE))
      {
        media++;
      }
      leituras++;
      if (media >= 10)
      {
        leituras = 25;
      }
    }

    if (leituras >= 20)
    {
      if (media >= 10)
      {
        ultimoEstado = !ultimoEstado;
        stateLED = !stateLED;
        report(stateLED);
      }
      media = 0;
      leituras = 0;
      lendo = false;
    }
    delayLeitura = millis();
  }
}

void interrupcao()
{
  if (!lendo)
  {
    lendo = true;
  }
}

void report(bool stateLED)
{
  if (stateLED)
  {
    sonoff.reportController("on", "1");
  }
  else
  {
    sonoff.reportController("on", "0");
  }
}