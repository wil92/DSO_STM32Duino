/***************************************************
 STM32 duino based firmware for DSO SHELL/150
 *  * GPL v2
 * (c) mean 2019 fixounet@free.fr
 ****************************************************/

#include "dso_global.h"
#include "dso_adc.h"
#include "dso_eeprom.h"
extern DSOADC                     *adc;
extern float                       voltageFineTune[16];
extern uint16_t directADC2Read(int pin);
extern VoltageSettings vSettings[];
static float fvcc=0;

#define SHORT_PRESS(x) (controlButtons->getButtonEvents(DSOControl::x)&EVENT_SHORT_PRESS)

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
static int averageADC2Read()
{
    int sum=0;
    for(int i=0;i<NB_SAMPLES;i++)
    {
        sum+=directADC2Read(analogInPin);
        xDelay(2);
    }
    sum=(sum+(NB_SAMPLES/2)-1)/NB_SAMPLES;
    return sum;
}


void doCalibrate(uint16_t *array,int color, const char *txt,DSOControl::DSOCoupling target)
{
    
    printCalibrationTemplate("Connect probe to ground","(connect the 2 crocs together)");
    header(color,txt,target); 
    
    for(int range=0;range<14;range++)
    {
        controlButtons->setInputGain(range);        
        xDelay(10);
        array[range]=averageADC2Read();
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
    
    
    adc->setupADCs ();
    adc->setTimeScale(ADC_SMPR_1_5,ADC_PRE_PCLK2_DIV_2); // 10 us *1024 => 10 ms scan
    printCalibrationTemplate("Connect the 2 crocs together.","");
    waitOk();    
    doCalibrate(calibrationDC,YELLOW,"Set switch to *DC*",DSOControl::DSO_COUPLING_DC);       
    doCalibrate(calibrationAC,GREEN, "Set switch to *AC*",DSOControl::DSO_COUPLING_AC);
    //while(1) {};
    DSOEeprom::write();         
    tft->fillScreen(0);
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
    DSOCapture::DSO_VOLTAGE_RANGE range;    
    int         tableOffset;            // offset in fineVoltageMultiplier table
    float       expectedVoltage;
};
/**
 */
MyCalibrationVoltage myCalibrationVoltage[]=
{
   {"10.0v",DSOCapture::DSO_VOLTAGE_2V,    9,  10.0},    // 2v/div range
    {"5.0v",DSOCapture::DSO_VOLTAGE_1V,    8,  5.0},     // 1v/div range
    {"2.5v",DSOCapture::DSO_VOLTAGE_500MV, 7,  2.5},     // 500mv/div range
    {"1.0v",DSOCapture::DSO_VOLTAGE_200MV, 6,  1.0},     // 200mv/div range
    {"500mV",DSOCapture::DSO_VOLTAGE_100MV,5,  0.5},     // 100mv/div range
    {"250mV",DSOCapture::DSO_VOLTAGE_50MV, 4,  .25},    // 2v/div range
    {"100mV",DSOCapture::DSO_VOLTAGE_20MV, 3,  0.1},     // 1v/div range    
//    {"50mV",DSOCapture::DSO_VOLTAGE_10MV,  2,  0.05},     // 500mv/div range
//    {"25mV",DSOCapture::DSO_VOLTAGE_5MV,   1,  0.025},     // 200mv/div range
//    {"5mv",DSOCapture::DSO_VOLTAGE_1MV,   0,   0.005},     // 100mv/div range
};
float performVoltageCalibration(const char *title, float expected,float defalt,int offset);
/**
 * 
 * @return 
 */
bool DSOCalibrate::voltageCalibrate()
{
    fvcc=DSOADC::getVCCmv();
    tft->setFontSize(Adafruit_TFTLCD_8bit_STM32::MediumFont);  
    tft->setTextColor(WHITE,BLACK);
    
    
    tft->fillScreen(BLACK);
    printxy(0,5,"===VOLT CALIBRATION====");
    printxy(0,30,"Set Input to DC");
    printxy(0,50,"and press OK");
    
    while(1)
    {
        controlButtons->updateCouplingState();
        DSOControl::DSOCoupling   newcpl=controlButtons->getCouplingState(); 
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
        int range=myCalibrationVoltage[i].tableOffset;
        capture->setVoltageRange(myCalibrationVoltage[i].range);
        float f=performVoltageCalibration(myCalibrationVoltage[i].title,
                                          myCalibrationVoltage[i].expectedVoltage,
                                          vSettings[range].multiplier,
                                          vSettings[range].offset[0]);
        if(f)
            voltageFineTune[range]=(f*4096000.)/fvcc;
        else
            voltageFineTune[range]=0;
    }    
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
}

/**
 * 
 * @param title
 * @param expected
 * @return 
 */
float performVoltageCalibration(const char *title, float expected,float defalt,int offset)
{
#define SCALEUP 1000000    
    fineHeader(title);
    while(1)
    {   // Raw read
        int sum=averageADC2Read();        
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
         
        if( SHORT_PRESS(DSO_BUTTON_OK))
             return f;
        if( SHORT_PRESS(DSO_BUTTON_VOLTAGE))
             return 0.;
    }    
}
