#include "App.h"
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <pcf8574.h>
#include <lcd.h>
#include <softTone.h>
#include <math.h>
#include <iostream>
#include <curl/curl.h>
#include <sstream>
#include <nlohmann/json.hpp>
#include <cstdio>

#define buzzerUser 22  // GPIO6 -> WiringPi Pin 22
#define buzzerMagician 26 // GPIO12 -> WiringPi Pin 26
#define ledUser 25     // GPIO26 -> WiringPi Pin 25
#define ledMagician 28  // GPIO20 -> WiringPi Pin 28

#define BASE 64         // BASE any number above 64

//Define the output pins of the PCF8574, which are directly connected to the LCD1602 pin.
#define RS      BASE+0
#define RW      BASE+1
#define EN      BASE+2
#define LED     BASE+3
#define D4      BASE+4
#define D5      BASE+5
#define D6      BASE+6
#define D7      BASE+7

int pcf8574_address = 0x27;        // PCF8574T:0x27, PCF8574AT:0x3F
int lcdhd; // used to handle LCD
const std::string API_KEY = "XXX"; //open router api
struct PerSocketData {};

//CURL FOR HTTP REQUESTS
CURL *curl;
CURLcode res;

std::string readBuffer;
std::string jsonResult;

struct curl_slist *headers = NULL;

//UTILITIES
void alertor(int pin) {
    int magicNotes[] = { 660, 880, 990, 1320, 1100, 990, 660 };
    int length = sizeof(magicNotes) / sizeof(int);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < length; j++) {
            softToneWrite(pin, magicNotes[j]);
            delay(100); // короткий трепет
        }
    }
    softToneWrite(pin, 0);
}


enum Status {
    USER, MAGICIAN
};

void buzz(Status status) {
    if (status == Status::USER) {
        digitalWrite(ledUser, HIGH);

        delay(1000);

        digitalWrite(ledUser, LOW);

    } else {
        alertor(buzzerMagician); //should be magician's buzzer
        digitalWrite(ledMagician, HIGH);

        delay(1000);

        digitalWrite(ledMagician, LOW);

    }

}

int detectI2C(int addr) {
    int _fd = wiringPiI2CSetup(addr);
    if (_fd < 0) {
        printf("Error address : 0x%x \n", addr);
        return 0;
    }
    else {
        if (wiringPiI2CWrite(_fd, 0) < 0) {
            printf("Not found device in address 0x%x \n", addr);
            return 0;
        }
        else {
            printf("Found device in address 0x%x \n", addr);
            return 1;
        }
    }
}

void writeDisplay(std::string msg) {
    lcdClear(lcdhd);
    
    if (msg.length() > 16) {
        lcdPosition(lcdhd, 0, 0);
        lcdPrintf(lcdhd, std::string(msg.substr(0, 16)).c_str());

        lcdPosition(lcdhd, 0, 1);
        lcdPrintf(lcdhd, std::string(msg.substr(16)).c_str());
    } else {
        lcdPosition(lcdhd, 0, 0);
        lcdPrintf(lcdhd, std::string(msg).c_str());
    }
}

void get_host_ip() {
    FILE* fp = popen("ip -4 addr show wlan0 | grep -oP '(?<=inet\\s)\\d+(\\.\\d+){3}'", "r");
    if (!fp) {
        std::cerr << "Ошибка при запуске команды!" << std::endl;
        return;
    }

    char buffer[128];
    bool found_ip = false;

    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove newline
        std::cout << "IP: " << buffer << std::endl;
        found_ip = true;
    }

    if (!found_ip) {
        std::cerr << "Не удалось получить IP-адрес!" << std::endl;
    }


    int code = pclose(fp);
    if ((code >> 8) != 0) {
        std::cerr << "Команда завершилась с ошибкой, код: " << code << "Повтор ..." << std::endl;
        delay(3000);
        get_host_ip();
    } else {
        writeDisplay(std::string(buffer));
        std::cout << "Код возврата: " << code << std::endl;
    }
}

int initialize() {

    int i;
    printf("Program is starting ...\n");

    wiringPiSetup();
    
    if (detectI2C(0x27)) {
        pcf8574_address = 0x27;

    } else if (detectI2C(0x3F)) {
        pcf8574_address = 0x3F;

    } else {
        printf("No correct I2C address found, \n"
            "Please use command 'i2cdetect -y 1' to check the I2C address! \n"
            "Program Exit. \n");
        return -1;

    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    pcf8574Setup(BASE, pcf8574_address); //initialize PCF8574

    pinMode(buzzerMagician, OUTPUT);
    softToneCreate(buzzerMagician);
    pinMode(buzzerUser, OUTPUT);

    pinMode(ledUser, OUTPUT);
    pinMode(ledMagician, OUTPUT);

    for (i = 0; i < 8; i++) {
        pinMode(BASE + i, OUTPUT);
        //set PCF8574 port to output mode
    }

    digitalWrite(LED, HIGH); //turn on LCD backlight
    digitalWrite(RW, LOW); //allow writing to LCD

    lcdhd = lcdInit(2, 16, 4, RS, EN, D4, D5, D6, D7, 0, 0, 0, 0); // initialize LCD and return “handle”
    if (lcdhd == -1) printf("lcdInit failed !");

    get_host_ip();    
    return 1;
}




//HTTP REQUESTS

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int curlSetup(){
        
    if(curl){
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://openrouter.ai/api/v1/chat/completions");
        std::string bearer = "Authorization: Bearer " + API_KEY;
        
        //append to header
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, bearer.c_str());
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        return 1;
    }
    return 0;
}

void sendMessage(std::string message) {
    // очищаем readBuffer перед каждым новым запросом
    readBuffer.clear();

    if (!curlSetup()) return;

    std::ostringstream oss;

    oss << R"({
    "model": "deepseek/deepseek-chat-v3-0324:free",
    "messages": [
        {
            "role": "user",
            "content": "You are a helpful magician who should give short answers (max 32 chars). Answer this: )" << message << R"("
        }
    ]
    })";

    std::string json_data = oss.str();

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        
    } else {
        std::cout << "Ответ от сервера: " << readBuffer << std::endl;

        // разбираем JSON-ответ
        try {
            auto json_response = nlohmann::json::parse(readBuffer);
            std::string reply = json_response["choices"][0]["message"]["content"];
            std::cout << "Ответ: " << reply << std::endl;
            jsonResult = reply;

            buzz(Status::MAGICIAN);
            writeDisplay(reply);  // покажем на экране

        } catch (const std::exception &e) {
            std::cerr << "Ошибка при парсинге JSON: " << e.what() << std::endl;
        }
    }

    // очищаем список заголовков, но НЕ сам curl
    curl_slist_free_all(headers);
    headers = nullptr;  // чтобы не использовать освобожденный указатель
    
}


int main(void) {
    if (!initialize()) return 1;
    

    uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
            writeDisplay("I am a magician! Ask questions");
            std::cout << "🔌 Клиент подключился!" << std::endl;
        },
        .message = [](auto *ws, std::string_view msg, uWS::OpCode) {
            buzz(Status::USER);
            std::cout << "📩 Сообщение: " << msg << std::endl;
            writeDisplay("Thinking ...");
            sendMessage(std::string(msg));            
            
            ws->send(jsonResult);
        },

        .close = [](auto *ws, int code, std::string_view msg) {
            writeDisplay("Bye !! :(");
            std::cout << "❌ Клиент отключился" << std::endl;
            
        }
    }).listen(9001, [](auto *token) {
        if (token) std::cout << "✅ Сервер слушает порт 9001" << std::endl;
        else std::cout << "❌ Не удалось запустить сервер" << std::endl;
    }).run();

        curl_easy_cleanup(curl);
        curl_global_cleanup();

    return 0;
}
