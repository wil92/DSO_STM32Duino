
/**
 Pin usage
 * 
 * PC13/14/15 OUTPUT LCD control 
 * PB0--PB7   OUTPUT LCD Databus
 * PA0        x      ADCIN
 * PA6        OUTPUT LCD nRD
 * PA7        OUTPUT Test signal
 * PA8        INPUT  Trig
 * PB0--B1    INPUT Rotary encoder
 * PB3--B7    INPUT Buttons
 * 
 * PB8        x      TL_PWM
 * PB9        OUTPUT LCD RESET
 * PB12       OUTPUT AMPSEL
 * 
 
 * PA1..A4    x      SENSEL
 * PA5        INPUT  CPLSEL
 * 
 
 
 */
// Rotary encoder part : Derived from  Rotary encoder handler for arduino * Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3. * Contact: bb@cactii.net
// Debounce part derived from http://www.kennethkuhn.com/electronics/debounce.c


#include <Wire.h>
#include "MapleFreeRTOS1000_pp.h"
#include "dsoControl.h"
#include "dsoControl_internal.h"

#define TICK                  10
#define LONG_PRESS_THRESHOLD (1000/TICK)
#define SHORT_PRESS_THRESHOLD (2)
#define HOLDOFF_THRESHOLD     (50/TICK)
#define COUNT_MAX             3

 enum DSOButtonState
  {
    StateIdle=0,
    StatePressed=1,
    StateLongPressed=2,
    StateHoldOff=3
  };
#define ButtonToPin(x)    (PB0+x)
#define pinAsInput(x)     pinMode(ButtonToPin(x),INPUT_PULLUP);
#define attachRE(x)       attachInterrupt(ButtonToPin(x),_myInterruptRE,(void *)x,FALLING );

#define NB_BUTTONS 8

  
/**
 */
class singleButton
{
public:
                    singleButton()
                    {
                        _state=StateIdle;
                        _events=0;
                        _holdOffCounter=0;
                        _pinState=0;
                        _pinCounter=0;
                    }
                    bool holdOff() // Return true if in holdoff mode
                    {
                        if(_state!=StateHoldOff)
                            return false;
                        _holdOffCounter++;
                        if(_holdOffCounter>HOLDOFF_THRESHOLD)
                        {
                            _state=StateIdle;
                            return false;
                        }
                        return true;
                    }
                    void goToHoldOff()
                    {
                        _state=StateHoldOff;
                        _holdOffCounter=0;
                    }
                    void integrate(bool k)
                    {         
                        // Integrator part
                        if(k)
                        {
                            _pinCounter++;
                        }else
                        {
                            if(_pinCounter) 
                            {
                                if(_pinCounter>=COUNT_MAX) _pinCounter=COUNT_MAX-1;
                                else
                                    _pinCounter--;
                            }
                        }
                    }
                    
                    void runMachine(int oldCount)
                    {
                        
                        int oldPin=_pinState;
                        int newPin=_pinCounter>(COUNT_MAX-1);

                        int s=oldPin+oldPin+newPin;
                        switch(s)
                        {
                            default:
                            case 0: // flat
                                break;
                            case 2:
                            { // released
                                if(_state==StatePressed)
                                {                        
                                    if(oldCount>SHORT_PRESS_THRESHOLD)
                                    {
                                        _events|=EVENT_SHORT_PRESS;
                                    }
                                }
                                goToHoldOff();
                                break;
                            }
                            case 1: // Pressed
                                _state=StatePressed;
                                break;
                            case 3: // Still pressed
                                if(_pinCounter>LONG_PRESS_THRESHOLD && _state==StatePressed) // only one long
                                {
                                    _state=StateLongPressed;
                                    _events|=EVENT_LONG_PRESS;                                    
                                }
                                break;
                        }                       
                        _pinState=newPin;
                    }        
                    
    DSOButtonState _state;
    int            _events;
    int            _holdOffCounter;
    int            _pinState;
    int            _pinCounter;
};
  
static DSOControl  *instance=NULL;

static singleButton _buttons[NB_BUTTONS];

static int state;  // rotary state
static int counter; // rotary counter

static TaskHandle_t taskHandle;




/**
 * \brief This one is for left/right
 * @param a
 */
static void _myInterruptRE(void *a)
{
    instance->interruptRE(!!a);
}


/**
 * 
 */
DSOControl::DSOControl()
{
    state = R_START;
    instance=this;
    counter=0;
    pinAsInput(DSO_BUTTON_UP);
    pinAsInput(DSO_BUTTON_DOWN);
    
    pinAsInput(DSO_BUTTON_ROTARY);
    pinAsInput(DSO_BUTTON_VOLTAGE);
    pinAsInput(DSO_BUTTON_TIME);
    pinAsInput(DSO_BUTTON_TRIGGER);
    pinAsInput(DSO_BUTTON_OK);
    
}


static void trampoline(void *a)
{
    DSOControl *ctrl=(DSOControl*)a;
    ctrl->runLoop();
}


/**
 * 
 */
void DSOControl::runLoop()
{
    xDelay(5);
    while(1)
    {
        xDelay(TICK);
        uint32_t val= GPIOB->regs->IDR;     
        for(int i=DSO_BUTTON_ROTARY;i<=DSO_BUTTON_OK;i++)
        {
            singleButton &button=_buttons[i];
            if(button.holdOff()) 
                continue;
            
            int k=!(val&(1<<i));
            
            int oldCount=button._pinCounter;
            button.integrate(k);
            button.runMachine(oldCount);
          
        }        
    }
}

/**
 * 
 * @return 
 */
bool DSOControl::setup()
{
    attachRE(DSO_BUTTON_UP);
    attachRE(DSO_BUTTON_DOWN);
    xTaskCreate( trampoline, "Control", 150, this, 15, &taskHandle );       
}
/**
 * 
 * @param a
 */
void DSOControl::interruptRE(int a)
{
   
  // Grab state of input pins.
  int pinstate =  ( GPIOB->regs->IDR)&3;
  // Determine new state from the pins and state table.
  state = ttable[state & 0xf][pinstate];
  // Return emit bits, ie the generated event.
  switch(state&DIR_MASK)
  {
    case DIR_CW:
            counter--;
            break;
    case DIR_CCW: 
            counter++;
            break;
    default: 
            break;
  }
  
}
/**
 * 
 * @param button
 * @return 
 */
bool DSOControl::getButtonState(DSOControl::DSOButton button)
{
    return _buttons[button]._pinState;
}
/**
 * 
 * @param button
 * @return 
 */
int  DSOControl::getButtonEvents(DSOButton button)
{
    noInterrupts(); // very short, better than sem ?
    int evt=_buttons[button]._events;
    _buttons[button]._events=0;
    interrupts();
    return evt;
}

/**
 * 
 * @return 
 */
int  DSOControl::getRotaryValue()
{
    int r;
    noInterrupts();
    r=counter;
    counter=0;
    interrupts();
    return r;
}
//