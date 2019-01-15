// Graphics library by ladyada/adafruit with init code from Rossum
// MIT license

#include "Adafruit_TFTLCD_8bit_STM32.h"
#include "Adafruit_TFTLCD_8bit_STM32_priv.h"

#include "ili932x.h"
#include "ili9341.h"
#include "hx8347g.h"
#include "hx8357x.h"

//gpio_reg_map * cntrlRegs;
gpio_reg_map * dataRegs;
uint32_t intReg;
uint32_t opReg;

/*****************************************************************************/
// Constructor
/*****************************************************************************/
Adafruit_TFTLCD_8bit_STM32 :: Adafruit_TFTLCD_8bit_STM32(void)
: Adafruit_GFX(TFTWIDTH, TFTHEIGHT)
{
}


/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::reset(void)
{

	dataRegs = TFT_DATA_PORT->regs;
	//Set control lines as output
	pinMode(TFT_RD, OUTPUT);
	pinMode(TFT_WR, OUTPUT);
	pinMode(TFT_RS, OUTPUT);
	pinMode(TFT_CS, OUTPUT);
        
        GPIOA->regs->CRL=0xa2022220U;
        GPIOA->regs->CRH=0x288004b4U;
  
        GPIOB->regs->CRL=0x88888888U;
        GPIOB->regs->CRH=0x8884ff1aU;

        GPIOC->regs->CRL=0x44444444U;
        GPIOC->regs->CRH=0x22244444U;
        
	CS_IDLE; // Set all control bits to HIGH (idle)
	CD_DATA; // Signals are ACTIVE LOW
	WR_IDLE;
	RD_IDLE;
/* testing PB4 - sometimes reserved by debug port, see http://www.stm32duino.com/viewtopic.php?f=35&t=1130&p=24289#p24289
	pinMode(PB4, OUTPUT);
	digitalWrite(PB4, HIGH);
	while (1) {
		CS_ACTIVE;
		//WR_STROBE;
		digitalWrite(PB4, LOW);
		digitalWrite(PB4, HIGH);
		CS_IDLE;
		delay(1000);
	}
*/
	//set up 8 bit parallel port to write mode.
	setWriteDir();

	// toggle RST low to reset
	if (TFT_RST > 0) {
		pinMode(TFT_RST, OUTPUT);
		digitalWrite(TFT_RST, HIGH);
		delay(100);
		digitalWrite(TFT_RST, LOW);
		delay(100);
		digitalWrite(TFT_RST, HIGH);
		delay(100);
	}
}




/*****************************************************************************/
// Draw an image bitmap (16bits per color) at the specified position from the provided buffer.
/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t * bitmap)
{
	if ( x>=0 && (x+w)<_width && y>=0 && (y+h)<=_height ) {
		// all pixel visible, do it in the fast way
		setAddrWindow(x,y,x+w-1,y+h-1);
		pushColors((uint16_t*)bitmap, w*h, true);
	} else {
		// some pixels outside visible area, do it in the classical way to disable off-screen points
		int16_t i, j;
		uint16_t * colorP = (uint16_t*)bitmap;
		for(j=0; j<h; j++) {
			for(i=0; i<w; i++ ) {
				drawPixel(x+i, y+j, *colorP++);
			}
		}
	}
}

/*****************************************************************************/
uint8_t read8_(void)
{
  RD_ACTIVE;
  delayMicroseconds(10);
  uint8_t temp = ( (dataRegs->IDR>>TFT_DATA_SHIFT) & 0x00FF);
  delayMicroseconds(10);
  RD_IDLE;
  delayMicroseconds(10);
  return temp;
}

/*****************************************************************************/
inline void writeCommand(uint16_t c)
{
	CS_ACTIVE;
        CD_COMMAND;
	write8(c>>8);
	write8(c);
}

/*****************************************************************************/
uint16_t Adafruit_TFTLCD_8bit_STM32::readID(void)
{  
    reset();
  uint32_t displayId=(readReg(0xdb)<<16)+readReg(0xda);
  uint32_t lowId=readReg32(0x04) ;
  
  if(lowId==0xff858552) // st7789 , really not sure it's the right way to do it
  {
     return 0x7789;
  }  
  
  if (lowId == 0x8000) 
  { // eh close enough
    // setc!
    writeRegister24(HX8357D_SETC, 0xFF8357);
    delay(300);
    //Serial.println(readReg(0xD0), HEX);
    if (readReg32(0xD0) == 0x990000) {
      return 0x8357;
    }
  }
    
  uint16_t id = readReg32(0xD3);
  if (id != 0x9341 && id != 0x9338) 
  {
    id = readReg(0);
  }
	//Serial.print("ID: "); Serial.println(id,HEX);
  return id;
}

/*****************************************************************************/
uint32_t readReg32(uint8_t r)
{
  uint32_t id;
  uint8_t x;

  // try reading register #4
  writeCommand(r);
  setReadDir();  // Set up LCD data port(s) for READ operations
  CD_DATA;
  delayMicroseconds(50);
  read8(x);
  id = x;          // Do not merge or otherwise simplify
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id  |= x;        // shenanigans that are going on.
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id  |= x;        // shenanigans that are going on.
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id  |= x;        // shenanigans that are going on.
  CS_IDLE;
  setWriteDir();  // Restore LCD data port(s) to WRITE configuration
  return id;
}
/*****************************************************************************/
uint16_t readReg(uint8_t r)
{
  uint16_t id;
  uint8_t x;

  writeCommand(r);
  setReadDir();  // Set up LCD data port(s) for READ operations
  CD_DATA;
  delayMicroseconds(10);
  read8(x);
  id = x;          // Do not merge or otherwise simplify
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id |= x;        // shenanigans that are going on.
  CS_IDLE;
  setWriteDir();  // Restore LCD data port(s) to WRITE configuration

  //Serial.print("Read $"); Serial.print(r, HEX); 
  //Serial.print(":\t0x"); Serial.println(id, HEX);
  return id;
}

/*****************************************************************************/
void writeRegister8(uint16_t a, uint8_t d)
{
  writeCommand(a);
  CD_DATA;
  write8(d);
  CS_IDLE;
}

/*****************************************************************************/
void writeRegister16(uint16_t a, uint16_t d)
{
  writeCommand(a);
  CD_DATA;
  write8(d>>8);
  write8(d);
  CS_IDLE;
}

/*****************************************************************************/
void writeRegisterPair(uint16_t aH, uint16_t aL, uint16_t d)
{
  writeRegister8(aH, d>>8);
  writeRegister8(aL, d);
}

/*****************************************************************************/
void writeRegister24(uint16_t r, uint32_t d)
{
  writeCommand(r); // includes CS_ACTIVE
  CD_DATA;
  write8(d >> 16);
  write8(d >> 8);
  write8(d);
  CS_IDLE;
}

/*****************************************************************************/
void writeRegister32(uint16_t r, uint32_t d)
{
  writeCommand(r);
  CD_DATA;
  write8(d >> 24);
  write8(d >> 16);
  write8(d >> 8);
  write8(d);
  CS_IDLE;
}

/****************************************************************************
void writeRegister32(uint16_t r, uint16_t d1, uint16_t d2)
{
  writeCommand(r);
  CD_DATA;
  write8(d1 >> 8);
  write8(d1);
  write8(d2 >> 8);
  write8(d2);
  CS_IDLE;
}
*/
//Adafruit_TFTLCD_8bit_STM32 tft;


/*****************************************************************************/
// Fast block fill operation for fillScreen, fillRect, H/V line, etc.
// Requires setAddrWindow() has previously been called to set the fill
// bounds.  'len' is inclusive, MUST be >= 1.
/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::flood(uint16_t color, uint32_t len)
{
  uint16_t blocks;
  uint8_t  i, hi = color >> 8,
              lo = color;

  CS_ACTIVE_CD_COMMAND;
  floodPreamble();
  

  // Write first pixel normally, decrement counter by 1
  CD_DATA;
  write8(hi);
  write8(lo);
  len--;

  blocks = (uint16_t)(len / 64); // 64 pixels/block
  if(hi == lo) {
    // High and low bytes are identical.  Leave prior data
    // on the port(s) and just toggle the write strobe.
    while(blocks--) {
      i = 16; // 64 pixels/block / 4 pixels/pass
      do {
        WR_STROBE; WR_STROBE; WR_STROBE; WR_STROBE; // 2 bytes/pixel
        WR_STROBE; WR_STROBE; WR_STROBE; WR_STROBE; // x 4 pixels
      } while(--i);
    }
    // Fill any remaining pixels (1 to 64)
	i = len & 63;
    while (i--) {
		WR_STROBE; WR_STROBE;
	}
  } else {
    while(blocks--) {
      i = 16; // 64 pixels/block / 4 pixels/pass
      do {
        write8(hi); write8(lo); write8(hi); write8(lo);
        write8(hi); write8(lo); write8(hi); write8(lo);
      } while(--i);
    }
	i = len & 63;
    while (i--) { // write here the remaining data
      write8(hi); write8(lo);
    }
  }
  CS_IDLE;
}



/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::drawFastHLine(int16_t x, int16_t y, int16_t length, uint16_t color)
{
  int16_t x2;

  // Initial off-screen clipping
  if((length <= 0     ) ||
     (y      <  0     ) || ( y                  >= _height) ||
     (x      >= _width) || ((x2 = (x+length-1)) <  0      )) return;

  if(x < 0) {        // Clip left
    length += x;
    x       = 0;
  }
  if(x2 >= _width) { // Clip right
    x2      = _width - 1;
    length  = x2 - x + 1;
  }

  setAddrWindow(x, y, x2, y);
  flood(color, length);  
}


/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::drawFastVLine(int16_t x, int16_t y, int16_t length, uint16_t color)
{
  int16_t y2;

  // Initial off-screen clipping
  if((length <= 0      ) ||
     (x      <  0      ) || ( x                  >= _width) ||
     (y      >= _height) || ((y2 = (y+length-1)) <  0     )) return;
  if(y < 0) {         // Clip top
    length += y;
    y       = 0;
  }
  if(y2 >= _height) { // Clip bottom
    y2      = _height - 1;
    length  = y2 - y + 1;
  }

  setAddrWindow(x, y, x, y2);
  flood(color, length); 
}


/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::fillRect(int16_t x1, int16_t y1, int16_t w, int16_t h, uint16_t fillcolor)
{
	//Serial.println("\n::fillRect...");
  int16_t  x2, y2;

  // Initial off-screen clipping
  if( (w            <= 0     ) ||  (h             <= 0      ) ||
      (x1           >= _width) ||  (y1            >= _height) ||
     ((x2 = x1+w-1) <  0     ) || ((y2  = y1+h-1) <  0      )) return;
  if(x1 < 0) { // Clip left
    w += x1;
    x1 = 0;
  }
  if(y1 < 0) { // Clip top
    h += y1;
    y1 = 0;
  }
  if(x2 >= _width) { // Clip right
    x2 = _width - 1;
    w  = x2 - x1 + 1;
  }
  if(y2 >= _height) { // Clip bottom
    y2 = _height - 1;
    h  = y2 - y1 + 1;
  }

  setAddrWindow(x1, y1, x2, y2);
  flood(fillcolor, (uint32_t)w * (uint32_t)h); 
}

/*****************************************************************************/
// Issues 'raw' an array of 16-bit color values to the LCD; used
// externally by BMP examples.  Assumes that setWindowAddr() has
// previously been set to define the bounds.  Max 255 pixels at
// a time (BMP examples read in small chunks due to limited RAM).
/*****************************************************************************/
void Adafruit_TFTLCD_8bit_STM32::pushColors(uint16_t *data, int16_t len, boolean first)
{
  uint16_t color;
  uint8_t  hi, lo;
  CS_ACTIVE;
  if(first == true) 
  { // Issue GRAM write command only on first call
    CD_COMMAND;
    pushColorsPreamble();    
  }
  CD_DATA;
  while(len--) {
    color = *data++;
    hi    = color >> 8; // Don't simplify or merge these
    lo    = color;      // lines, there's macro shenanigans
    write8(hi);         // going on.
    write8(lo);
  }
  CS_IDLE;
}

#include "ili9341.h"
#include "st7789.h"

Adafruit_TFTLCD_8bit_STM32 *Adafruit_TFTLCD_8bit_STM32::spawn(int id)
{
    switch(id)
    {
        case 0x7789: return  new Adafruit_TFTLCD_8bit_STM32_ST7789;break;
        case 0x9341: return  new Adafruit_TFTLCD_8bit_STM32_ILI9341;break;
        default : return NULL;
    }
    return NULL;    
}
