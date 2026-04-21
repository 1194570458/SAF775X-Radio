#include "rds.h"
#include "stdbool.h"
#include "string.h"
#include "ctype.h"

#include "func.h"

#define RDS_ERR_MODE    RDS_LO

#define WORD_LOW_BYTE(a)     ((unsigned char)(a & 0x00FF))
#define WORD_HIGH_BYTE(a)    ((unsigned char)(a >> 8))

// PTY Definition [ 8 char ]
const unsigned char PTY_Table[32][9] =
{
  "PTY None",
  "News    ",
  "Affairs ",
  "Info    ",
  "Sport   ",
  "Educate ",
  "Drama   ",
  "Culture ",
  "Science ",
  "Varied  ",
  "Pop M   ",
  "Rock M  ",
  "Easy M  ",
  "Light M ",
  "Classics",
  "Other M ",
  "Weather ",
  "Finance ",
  "Children",
  "Social  ",
  "Religion",
  "Phone In",
  "Travel  ",
  "Leisure ",
  "Jazz    ",
  "Country ",
  "Nation M",
  "Oldies  ",
  "Folk M  ",
  "Document",
  "Test    ",
  "Alarm   "
};


struct RDSData* rds;
static uint16_t RTDataMask = 0;

ConfidenceFilter PI_filter, RT_filter;

static uint16_t FindMin(uint8_t *str, uint16_t lengh)
{
  uint16_t i = 0;
  uint16_t min = 0;
  for(i=0;i<lengh;i++)
  {
    if(str[min]>str[i])
      min = i;
  }
  return min;
}



static void CopyStr(unsigned char* des, const unsigned char* source, int len)
{
  memcpy(des, source, len);
}

static void FillStr(unsigned char* str, unsigned char ch, int len)
{
  memset(str, ch, len);
  str[len-1] = '\0';
}

/*
 * 0:no error
 * 1:small error
 * 2:large error
 * 3:uncorrectable error
 */
static inline uint8_t CheckError(struct RDSBuffer *rawdata, uint8_t block)
{
  return ( (rawdata->error >> block) & 0x03 );
}

static inline uint8_t CheckVersionType(struct RDSBuffer *rawdata)
{
  return ( (rawdata->status >> 4) & 0x01 );
}

static inline uint8_t CheckGroupType(struct RDSBuffer *rawdata)
{
  return ( (rawdata->block_B >> 12) & 0x000F );
}

void RDS_Init(struct RDSData* init)
{
  rds = init;
  
  rds->PI = 0;
  rds->PI_Available = false;
  FillStr(rds->PTY, ' ', 9);
  FillStr(rds->PS, ' ', 9);
  rds->PS_Available = false;
  FillStr(rds->RT[0], ' ', 65);
  FillStr(rds->RT[1], ' ', 65);
  rds->RT_Type = 0;
  rds->RT_Size[0] = 0;
  rds->RT_Size[1] = 0;
  rds->Hour = -1;
  rds->Minute = -1;
  rds->RDSFlag = 0;
  
  cfd_filter_init(&PI_filter);
  cfd_filter_init(&RT_filter);
}

void RDS_Refresh(void)
{
  rds->PI = 0;
  rds->PI_Available = false;
  FillStr(rds->PTY, ' ', 9);
  FillStr(rds->PS, ' ', 9);
  rds->PS_Available = false;
  rds->RT_Type = 0;
  if(rds->RT_Size[0] > 0 || rds->RT_Size[1] > 0) {
    FillStr(rds->RT[0], ' ', 65);
    FillStr(rds->RT[1], ' ', 65);
  }
  rds->Hour = -1;
  rds->Minute = -1;
  rds->RDSFlag = 0;

  cfd_filter_init(&PI_filter);
  cfd_filter_init(&RT_filter);
}

int ReadPI(struct RDSBuffer *rawdata)
{
  static int oldpi = 0;
  int newpi = 0;
  uint8_t error;
  
  if(CheckError(rawdata, BLOCK_A) > RDS_ERR_MODE) {
    return 1;
  }
  
  error = cfd_filter_update(&PI_filter, 3, rawdata->block_A, &newpi);
  if(error) {
    return 1;
  }
  
  if(newpi != oldpi) {
    oldpi = newpi;
    rds->PI = newpi;
    rds->RDSFlag |= ReadyBit_PI;
    rds->PI_Available = true;
  }
  return 0;
}

uint8_t ReadPTY(struct RDSBuffer *rawdata)
{
  uint8_t pty;
  static uint8_t oldpty = 0;
  pty = ( (rawdata->block_B >> 5) & 0x001F );
  
  if(pty != 0 && oldpty != pty)
  {
    oldpty = pty;
    rds->RDSFlag |= ReadyBit_PTY;
  }
  return pty;
}

void ReadPS(struct RDSBuffer *rawdata)
{
  static unsigned char oldps[8];
  uint8_t segment = 0;
  segment = (uint8_t)(rawdata->block_B & 0x0003);
  
  if(CheckError(rawdata, BLOCK_D) <= RDS_ERR_MODE)
  {
    rds->PS[segment*2] = WORD_HIGH_BYTE(rawdata->block_D);
    rds->PS[segment*2+1] = WORD_LOW_BYTE(rawdata->block_D);
    rds->PS[8] = '\0';
  }
  for(uint8_t i = 0;i<8;i++)
  {
    if(rds->PS[i] != oldps[i])
    {
      rds->RDSFlag |= ReadyBit_PS;
      rds->PS_Available = true;
      oldps[i] = rds->PS[i];
    }
  }
}

#include <stdio.h>
void ReadRT(struct RDSBuffer *rawdata)
{
  uint8_t segment = 0;
  int16_t tmp;
  int indi = 0;
  static uint8_t old_ind = 0;
  // segment = [ 0:15 ]
  segment = (uint8_t)(rawdata->block_B & 0x000F);
  indi = (rawdata->block_B >> 4) & 0x0001;
  
  if(cfd_filter_update(&RT_filter, 2, indi, &indi)) {
    return;  // unstable
  }
  
  if(indi != old_ind) {  // text a/b switched, clear saved msg
    printf("text a/b\n");
    old_ind = indi;
    FillStr(rds->RT[0], ' ', 65);
    FillStr(rds->RT[1], ' ', 65);
  }
  
  tmp = 0;
  if(CheckVersionType(rawdata) == VERSION_A)
  {
    rds->RT_Type = 0;
    if(CheckError(rawdata, BLOCK_C) <= RDS_ERR_MODE) {
      rds->RT[rds->RT_Type][segment*4]   = WORD_HIGH_BYTE(rawdata->block_C);
      rds->RT[rds->RT_Type][segment*4+1] = WORD_LOW_BYTE(rawdata->block_C);
      
      if(isgraph(rds->RT[rds->RT_Type][segment*4])) tmp = segment*4+1;
      if(isgraph(rds->RT[rds->RT_Type][segment*4+1])) tmp = segment*4+2;
      rds->RT_Size[rds->RT_Type] = max(tmp, rds->RT_Size[rds->RT_Type]);
      
      rds->RDSFlag |= ReadyBit_RT;
    }
    if(CheckError(rawdata, BLOCK_D) <= RDS_ERR_MODE) {
      rds->RT[rds->RT_Type][segment*4+2] = WORD_HIGH_BYTE(rawdata->block_D);
      rds->RT[rds->RT_Type][segment*4+3] = WORD_LOW_BYTE(rawdata->block_D);
      
      if(isgraph(rds->RT[rds->RT_Type][segment*4])) tmp = segment*4+3;
      if(isgraph(rds->RT[rds->RT_Type][segment*4+1])) tmp = segment*4+4;
      rds->RT_Size[rds->RT_Type] = max(tmp, rds->RT_Size[rds->RT_Type]);
      
      rds->RDSFlag |= ReadyBit_RT;
    }
  }
  else
  {
    rds->RT_Type = 1;
    if(CheckError(rawdata, BLOCK_D) <= RDS_ERR_MODE)
    {
      rds->RT[rds->RT_Type][segment*2] = WORD_HIGH_BYTE(rawdata->block_D);
      rds->RT[rds->RT_Type][segment*2+1] = WORD_LOW_BYTE(rawdata->block_D);
      
      if(isgraph(rds->RT[rds->RT_Type][segment*4])) tmp = segment*2+1;
      if(isgraph(rds->RT[rds->RT_Type][segment*4+1])) tmp = segment*2+2;
      rds->RT_Size[rds->RT_Type] = max(tmp, rds->RT_Size[rds->RT_Type]);
      
      rds->RDSFlag |= ReadyBit_RT;
    }
  }
  
}

void ReadCT(struct RDSBuffer *rawdata)
{
  uint8_t temp = 0;
  int8_t temp_h = 0;
  int8_t offset_h = 0;
  if((CheckError(rawdata, BLOCK_C) <= RDS_ERR_MODE) && (CheckError(rawdata, BLOCK_D) <= RDS_ERR_MODE))
  {
    offset_h = (rawdata->block_D & 0x001F)/2;
    if( ((rawdata->block_D >> 5) & 0x0001) )
      offset_h = -offset_h;
    
    rds->Minute = ((rawdata->block_D >> 6) & 0x003F);
    temp = ((rawdata->block_D >> 12) & 0x000F);
    temp |= ((rawdata->block_C << 4) & 0x0010);
    
    temp_h = temp + offset_h;
    if(temp_h < 0)
      rds->Hour = temp_h+24;
    else if(temp_h >= 24)
      rds->Hour = temp_h-24;
    else
      rds->Hour = temp_h;
    
    rds->RDSFlag |= ReadyBit_CT;
  }
}

void DecodeRDS(struct RDSBuffer *rawdata)
{
  ReadPI(rawdata);
  
  if(CheckError(rawdata, BLOCK_B) <= RDS_ERR_MODE)
  {
//    CopyStr(rds->PTY, PTY_Table[ReadPTY(rawdata)]);
    
    switch(CheckGroupType(rawdata))
    {
      case 0:{  // Program Service name
        ReadPS(rawdata);
      };break;
      
      case 2:{  // Radio Text
        ReadRT(rawdata);
      };break;
      
//      case 4:{  // Clock-Time and Date
//        ReadCT(rawdata);
//      };break;
      
      default:break;
    }
  }
  
}




