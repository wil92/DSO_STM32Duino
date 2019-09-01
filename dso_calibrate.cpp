/***************************************************
 STM32 duino based firmware for DSO SHELL/150
 *  * GPL v2
 * (c) mean 2019 fixounet@free.fr
 ****************************************************/

#include "dso_global.h"
#include "dso_adc.h"
#include "dso_eeprom.h"
#include "dso_adc_gain.h"
#include "dso_adc_gain_priv.h"
#include "dso_gfx.h"
extern DSOADC                     *adc;

#define SHORT_PRESS(x) (controlButtons->getButtonEvents(DSOControl::x)&EVENT_SHORT_PRESS)
static uint16_t directADC2Read(int pin);
/**
 * 
 */
static void waitOk()
{
    while(!SHORT_PRESS(DSO_BUTTON_OK)) 
    {
        xDelay(10);
    }
}

/**
 * 
 * @param array
 */
static void printxy(int x, int y, const char *t)
{
    tft->setCursor(x, y);
    tft->myDrawString(t);
}
/**
  */
static void printCalibrationTemplate( const char *st1, const char *st2)
{
    tft->fillScreen(BLACK);  
    printxy(0,10,"======CALIBRATION========");
    printxy(40,100,st1);
    printxy(8,120,st2);
    printxy(80,200,"and press OK");
}

/**
 * 
 * @param cpl
 */
static void printCoupling(DSOControl::DSOCoupling cpl)
{
    static const char *coupling[3]={"current : GND","current : DC ","current : AC "};
    printxy(40,130,coupling[cpl]);
      
}

void header(int color,const char *txt,DSOControl::DSOCoupling target)
{
    printCalibrationTemplate(txt,"");
    DSOControl::DSOCoupling   cpl=(DSOControl::DSOCoupling)-1;
    printxy(220,40," switch /\\");
    while(1)
    {
            controlButtons->updateCouplingState();
            DSOControl::DSOCoupling   newcpl=controlButtons->getCouplingState(); 
            if(newcpl==target) 
                tft->setTextColor(GREEN,BLACK);
            else  
                tft->setTextColor(RED,BLACK);

            if(cpl!=newcpl)
            {
                printCoupling(newcpl);
                cpl=newcpl;
            }
            if(cpl==target && SHORT_PRESS(DSO_BUTTON_OK))
            {
                tft->setTextColor(BLACK,GREEN);
                printxy(160-8*8,160,"- processing -");
                tft->setTextColor(WHITE,BLACK);
                return;
            }
    }
}
#define NB_SAMPLES 64
/**
 * 
 * @return 
 */
static int averageADCRead()
{
    // Start Capture
    adc->prepareTimerSampling(1000); // 1Khz
    adc->startTimerSampling(200);
    FullSampleSet fset;
    while(!adc->getSamples(fset))
    {
        
    };
    int nb=fset.set1.samples;
    int sum=0;
    for(int i=0;i<nb;i++)
    {
        sum+=fset.set1.data[i];
    }
    sum=(sum+(nb/2)-1)/nb;
    return sum;
}


void doCalibrate(uint16_t *array,int color, const char *txt,DSOControl::DSOCoupling target)
{
    printCalibrationTemplate("Connect probe to ground","(connect the 2 crocs together)");
    header(color,txt,target);     
    for(int range=0;range<DSO_NB_GAIN_RANGES;range++)
    {
        DSOInputGain::setGainRange((DSOInputGain::InputGainRange) range);
        xDelay(10);
        array[range]=averageADCRead();
    }
}

/**
 * 
 * @return 
 */
bool DSOCalibrate::zeroCalibrate()
{    
    tft->setFontSize(Adafruit_TFTLCD_8bit_STM32::MediumFont);  
    tft->setTextColor(WHITE,BLACK);
          
    
    adc->setTimeScale(ADC_SMPR_1_5,ADC_PRE_PCLK2_DIV_2); // 10 us *1024 => 10 ms scan
    printCalibrationTemplate("Connect the 2 crocs together.","");
    waitOk();    
    doCalibrate(calibrationDC,YELLOW,"Set switch to *DC*",DSOControl::DSO_COUPLING_DC);       
    doCalibrate(calibrationAC,GREEN, "Set switch to *AC*",DSOControl::DSO_COUPLING_AC);    
    DSOEeprom::write();         
    tft->fillScreen(0);    
    printxy(20,100,"Restart the unit.");
    return true;        
}
/**
 * 
 * @return 
 */
bool DSOCalibrate::decalibrate()
{    
    DSOEeprom::wipe();
    return true;        
}
/**
 * 
 * @return 
 */
typedef struct MyCalibrationVoltage
{
    const char *title;
    float   expected;
    DSOInputGain::InputGainRange range;    
};
/**
 */
MyCalibrationVoltage myCalibrationVoltage[]=
{    
    {"24V",     24,     DSOInputGain::MAX_VOLTAGE_8V},    // 2v/div range
    {"16V",     16,     DSOInputGain::MAX_VOLTAGE_4V},    // 2v/div range
    {"8V",      8,      DSOInputGain::MAX_VOLTAGE_2V},     // 1v/div range
    //{"3.2V",    3.2,    DSOInputGain::MAX_VOLTAGE_800MV},     // 500mv/div range => Saturates
    //{"1.6V",    1.6,    DSOInputGain::MAX_VOLTAGE_400MV},     // 200mv/div range    => Saturates
    {"1V",      1,      DSOInputGain::MAX_VOLTAGE_250MV},     // 1v/div range
    {"800mV",   0.8,    DSOInputGain::MAX_VOLTAGE_200MV},     // 100mv/div range
    {"320mV",   0.32,   DSOInputGain::MAX_VOLTAGE_80MV},     //  50mv/div range
    {"150mV",   0.15,   DSOInputGain::MAX_VOLTAGE_40MV},    //   20mv/div range
    {"80mV",    0.08,   DSOInputGain::MAX_VOLTAGE_20MV},     //  10mv/div range    
};



float performVoltageCalibration(const char *title, float expected,float defalt,float previous,int offset);
/**
 * 
 * @return 
 */
bool DSOCalibrate::voltageCalibrate()
{
    float fvcc=DSOADC::getVCCmv();
    tft->setFontSize(Adafruit_TFTLCD_8bit_STM32::MediumFont);  
    tft->setTextColor(WHITE,BLACK);
    
    DSO_GFX::newPage("VOLT CALIBRATION");
    printxy(0,30,"Set Input to DC");
    printxy(0,50,"and press OK");
    
    while(1)
    {
        controlButtons->updateCouplingState();
        DSOControl::DSOCoupling   newcpl=controlButtons->getCouplingState(); 
        printCoupling(newcpl);
        if(newcpl==DSOControl::DSO_COUPLING_DC) 
        {
            waitOk();
            break;
        }
    }
    
    
    adc_set_sample_rate(ADC2, ADC_SMPR_239_5);
    int nb=sizeof(myCalibrationVoltage)/sizeof(MyCalibrationVoltage);
    for(int i=0;i<nb;i++)
    {                
        DSOInputGain::InputGainRange  range=myCalibrationVoltage[i].range;
        DSOInputGain::setGainRange(range);
        float expected=myCalibrationVoltage[i].expected;        
        int dex=(int)range;
        float previous=(voltageFineTune[dex]*fvcc)/4096000.;
        float f=performVoltageCalibration(myCalibrationVoltage[i].title,
                                          expected,
                                          DSOInputGain::getMultiplier(),
                                          previous,
                                          DSOInputGain::getOffset(0));
        if(f)
            voltageFineTune[dex]=(f*4096000.)/fvcc;
        else
            voltageFineTune[dex]=0;
    }    
    // If we have both 100mv and 2v
    DSOEeprom::write();         
    tft->fillScreen(0);
    return true;         
}

/**
 * 
 * @param expected
 */
static void fineHeader(const char *title)
{          
    tft->fillScreen(BLACK);
    printxy(0,5,"===VOLT CALIBRATION====");
    printxy(10,30,"Connect to ");
    tft->setTextColor(GREEN,BLACK);
    tft->myDrawString(title);
    
    tft->setTextColor(WHITE,BLACK);
    printxy(10,200,"Press ");
    tft->setTextColor(BLACK,WHITE);
    tft->myDrawString(" OK ");
    tft->setTextColor(WHITE,BLACK);
    tft->myDrawString("to set,");
    tft->setTextColor(BLACK,WHITE);
    tft->myDrawString(" Volt ");
    tft->setTextColor(WHITE,BLACK);
    tft->myDrawString(" for default");
    tft->setTextColor(BLACK,WHITE);
    tft->myDrawString(" Trigg ");
    tft->setTextColor(WHITE,BLACK);
    tft->myDrawString(" to keep current ");
     
    
}

/**
 * 
 * @param title
 * @param expected
 * @return 
 */
float performVoltageCalibration(const char *title, float expected,float defalt,float previous,int offset)
{
#define SCALEUP 1000000    
    fineHeader(title);
    while(1)
    {   // Raw read
        int sum=averageADCRead();        
        sum-=offset;               
        float f=expected;
        if(!sum) f=0;
        else
                 f=expected/sum;        
                      
        tft->setCursor(160, 90);
        tft->print(sum);                
        tft->setCursor(10, 90);
        tft->print(f*SCALEUP);
        tft->setCursor(10, 130);
        tft->print(defalt*SCALEUP);
         tft->setCursor(10, 160);
        tft->print(previous*SCALEUP);
        
         
        if( SHORT_PRESS(DSO_BUTTON_OK))
             return f;
        if( SHORT_PRESS(DSO_BUTTON_VOLTAGE))
             return 0.;
        if( SHORT_PRESS(DSO_BUTTON_TRIGGER))
             return previous;

        
    }    
}
