/**
 * @file classes.h
 * @author Martin Janček, mateusko.OAMDG@outlook.com 
 * @brief class definitions for sketch CasovySpinac.ino
 * @version 0.1
 * @date 2026-01-18
 * 
 * @copyright Copyright (c) 2026 Martin Janček
 * 
 */

#include <stdint.h>
#include <Arduino.h>
#include <U8g2lib.h>

#define DISPLAY_UPDATE_TIME 200

extern U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C  u8g2;

using Callback = int (*)();

// caka ds x 100ms, resetuje watchdog
void delay_ds(unsigned int ds) {
    for (unsigned int i = 0; i < ds; i++) {
    delay(100);
    __asm__ __volatile__("wdr"); // feed watchdog
  }
}

void feedWatchdog() {
    __asm__ __volatile__("wdr");
}

/// @brief States for PowerTimer
enum class TimerState : uint8_t {
  Start, Pause, Off
};

/// @brief states for Led
enum class LedState : uint8_t {
  Off, On, Blink
};

// --------------- C L A S S    L E D ------------------ //

class Led {
  public:
    Led(int pin, int onState = HIGH, int initialState = LOW) : pin(pin), onState(onState) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, initialState);
        isShining = onState == initialState;
        state = isShining ? LedState::On : LedState::Off;
    };

    void on() {
      isShining = true;
      digitalWrite(pin, onState);
      state = LedState::On;
    }

    void off() {
      isShining = false;
      digitalWrite(pin, onState == HIGH ? LOW : HIGH);
      state = LedState::Off;
    }
    
    void blink(unsigned long interval) {
      blinkInterval = interval;
      timer = millis();
      on();
      state = LedState::Blink;
    }

    void update() {
      if (state == LedState::Blink) {
        if (millis() - timer > blinkInterval) {
          timer = millis();
          if (isShining) off(); else on();
          state = LedState::Blink;
        }
      }
    }


  private:
    int pin;
    int onState;
    LedState state;
    bool isShining;
    unsigned long blinkInterval;
    unsigned long timer;
  
};

// --------------- C L A S S    SettingsArray ------------------ //

/// @brief Staticke pole. Typ položky T. Veľkosť pevná SIZE.
///        Má jednu predvolenú položku (index) 
/// @tparam T 
/// @tparam SIZE 
template <typename T, int SIZE>
class SettingsArray
{
    T data[SIZE]{};
    int index = 0;

public:
    int currentIndex() const { return index; }

    bool setIndex(int i)
    {
        if (i < 0 || i >= SIZE) return false;
        index = i;
        return true;
    }

    T& current() { return data[index]; }

    const T& current() const { return data[index]; }

    void setCurrent(const T& v) { data[index] = v; } // alebo použiješ current() = v;

    bool next(bool cyclic = false)
    {
        if (index + 1 >= SIZE) 
            if (cyclic) {
                index = 0;
                return true;
            }
            else {
                return false;
            }
        else {
            ++index;
            return true;
        }
    }

    bool prev(bool cyclic = false)
    {
        if (index - 1 < 0) 
            if (cyclic) {
                index = SIZE - 1;
                return true;
            }
            else {
                return false;
            }
        else {    
            --index;
            return true;
        }
    }

    T& operator[](int i) { return data[i]; }

    const T& operator[](int i) const {  return data[i]; }
};

// --------------- C L A S S    PowerTimer ------------------ //

/// @brief 
class PowerTimer {
    public:
        
        PowerTimer(int pin, bool logic = true) : pin(pin), logic(logic) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, logic ? LOW : HIGH); // power OFF
            time = 0;
            timer = 0;
            //freeze_timer = 0;
        }
        
        void start(int time) {
            this->time = time;
            timer = millis();
            digitalWrite(pin, logic ? HIGH : LOW); // power ON
        }


        void off() {
            while(1) {
                digitalWrite(pin, logic ? LOW : HIGH); // power OFF
                __asm__ __volatile__("wdr"); // feed watchdog (secure OFF)
            }
        }
        
        int getTime() { return time;}
        
        void freeze(int time) {
            freeze_time = time;
        }

        void update() {
            if (time <= 0) off();  // time's up, turn off 

            if (millis() - timer >= 1000ul) { // coundown timer "time" -1s
                timer = millis();
                if (freeze_time > 0) {
                    freeze_time --;
                } 
                else {
                    time--;
                }
            }
        }
        
    private:
        int pin; // I/O pin
        bool logic; // true = normal login ON=HIGH, OFF=LOW; false = on the contrary
        int time;  // countdown timer [s]

       
        unsigned long timer; // auxiliary timer
        int freeze_time; // countdown freeze timer

};

// --------------- C L A S S    Display ------------------ //
class Display {
    protected:
        int* time;
        Callback index_callback;
        Callback interval_callback;
        Callback time_callback;

        int tempTime = 0;
        unsigned long timer;    // update timer
        unsigned long timerT;   // temporary timer 
    
    
    public:
        Display(Callback time, Callback index, Callback interval) 
            : time_callback(time),
              index_callback(index),
              interval_callback(interval) 
        {
            timerT = 0;
        };

        void printTimeScreen() {
            char buffer[6]; 
            timer = millis();
            
            int time = time_callback();
            snprintf(buffer, sizeof(buffer), "%02d:%02d", time / 60, time % 60);

            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_logisoso28_tf);
            //u8g2.setFont(u8g2_font_7Segments_24x32_mn);
            u8g2.drawStr(5, 30, buffer);


            u8g2.setFont(u8g2_font_unifont_t_78_79);
            u8g2.setDrawColor(1);
            const uint16_t glyph = static_cast<uint16_t>(index_callback() + 1) +  0x277F;
            u8g2.drawGlyph(112, 15, glyph);

            u8g2.sendBuffer();
        }

        void printText(char* text) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_logisoso28_tf);
            u8g2.drawStr(5, 30, text);
            u8g2.sendBuffer();

        };

        void printInterval() {
            char buffer[8];
            int interval = interval_callback();
            
            snprintf(buffer, sizeof(buffer), "+%d min", interval);
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_logisoso28_tf);
            u8g2.drawStr(5, 30, buffer);
            u8g2.sendBuffer();
        };

        void freeze(unsigned long freezeTime) {
            timerT = millis();
            tempTime = freezeTime;
        }

        void update() {
            if(millis() - timer < DISPLAY_UPDATE_TIME) return;

            if (tempTime) { 
                if (millis() - timerT > tempTime) {
                    tempTime = 0;
                    printTimeScreen();
                }
                else 
                    return;
                
            }
            printTimeScreen();
        };
        

        bool doFreeze() {
            while(tempTime) {
                feedWatchdog();
                update();
            }
        }

    
};