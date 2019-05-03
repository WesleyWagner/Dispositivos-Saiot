//#include <Arduino.h>
#include <SaIoTDeviceLib.h>
#include "EmonLib.h" //INCLUSÃO DE BIBLIOTECA

#define timeToSend 1000 //TEMPO PARA ENVIAR OS DADOS (EM 'ms')

const int pinoSensor = A0; //PINO ANALÓGICO EM QUE O SENSOR ESTÁ CONECTADO
float ruido = 0.1; //RUÍDO PRODUZIDO NA SAÍDA DO SENSOR (DEVE SER AJUSTADO COM O SENSOR DESLIGADO APÓS CARREGAMENTO DO CÓDIGO)
unsigned long tDecorrido;
//String getHoraAtual();
 
EnergyMonitor emon1; //CRIA UMA INSTÂNCIA
 
WiFiClient espClient;
SaIoTDeviceLib tomada("VoltageDeviceTeste","VoltageDevice","ricardo@email.com");
SaIoTSensor medidorTensao("hd01234","tomada_02","V","number");
String senha = "12345678910";

void callback(char* topic, byte* payload, unsigned int length);
void Calcula_corrente();

void setup(){
  tomada.addSensor(medidorTensao);
  Serial.begin(115200);
  tomada.preSetCom(espClient, callback, 300);
  tomada.start(senha);
  emon1.voltage(pinoSensor, 270, 1.7);  // Voltage: input pin, calibration, phase_shift
	tDecorrido = millis();
}

void loop(){
  emon1.calcVI(20,500); //FUNÇÃO DE CÁLCULO (20 SEMICICLOS / TEMPO LIMITE PARA FAZER A MEDIÇÃO)
  float VoltageDraw = emon1.Vrms; //VARIÁVEL RECEBE O VALOR DE TENSÃO RMS OBTIDO
  VoltageDraw = VoltageDraw-ruido; //VARIÁVEL RECEBE O VALOR RESULTANTE DA TENSÃO RMS MENOS O RUÍDO

  if(VoltageDraw < 0){ //SE O VALOR DA VARIÁVEL FOR MENOR QUE 0, FAZ 
      VoltageDraw = 0; //VARIÁVEL RECEBE 0
  }
	if( abs((millis() - tDecorrido)) >= timeToSend ){
    Serial.print("Voltage: ");
    Serial.print(VoltageDraw);//MOSTRA NA SERIAL A TENSÃO ENVIADA
    Serial.println("V");
		medidorTensao.sendData(VoltageDraw,SaIoTCom::getDateNow()); 
		tDecorrido = millis();
	}
	tomada.handleLoop();
}

void callback(char* topic, byte* payload, unsigned int length){
  String payloadS;
  Serial.print("Topic: ");
  Serial.println(topic);
  for (unsigned int i=0;i<length;i++) {
    payloadS += (char)payload[i];
  }
  if(strcmp(topic,tomada.getSerial().c_str()) == 0){
    Serial.println("SerialLog: " + payloadS);
  }
 
}

