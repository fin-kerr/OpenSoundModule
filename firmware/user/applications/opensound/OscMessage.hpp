#ifndef __OscMessage_h__
#define __OscMessage_h__

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
// #include "opensound.h"

class OscMessage {
public:
  // address points to the start of the message buffer
  // todo: it's possible to reduce size of OscMessage
  // by making these 8 or 16bit indices instead of 32bit pointers
  uint8_t* address;
  uint8_t* types;
  uint8_t* data;
  uint8_t* end;
public:
  OscMessage() : address(NULL), types(NULL), data(NULL), end(NULL){}

  OscMessage(uint8_t* data, int size) {
    setBuffer(data, size);
    // parse();
  }

  void clear(){
    types = address;
    data = address;
    end = address;
  }

  int getAddressLength(){
    // includes 0 padding
    return types-address;
  }

  int getTypeTagLength(){
    return data-types;
  }

  int calculateAddressLength(){
    int size = strnlen((char*)address, end-address)+1;
    while(size & 0x03)
      size++;
    return size;
  }

  int calculateTypeTagLength(){
    int size = strnlen((char*)types, end-types)+1;
    while(size & 0x03)
      size++;
    return size;
  }

  int getPrefixLength(){
    return data-address;
  }

  int calculateMessageLength(){
    int size = getPrefixLength();
    for(int i=0; i<getSize(); ++i)
      size += getDataSize(i);
    return size;
   // return size+calculateDataLength();
  }

  uint8_t* getBuffer(){
    return address;
  }

  int getBufferSize(){
    // todo: 'end' specifies capacity, not end position
    return end-address;
  }

  void setBuffer(void* buffer, int length){
    // In a stream-based protocol such as TCP, the stream should begin with an int32 giving the size of the first packet, followed by the contents of the first packet, followed by the size of the second packet, etc.
    address = (uint8_t*)buffer;
    types = address;
    data = address;
    end = address+length;
  }

  // assumes address and end have been set
  void parse(){
    // Note: some older implementations of OSC may omit the OSC Type Tag string. Until all such implementations are updated, OSC implementations should be robust in the case of a missing OSC Type Tag String.
    types = address+calculateAddressLength();
    // while(*types == '\0' && ++types < end);
    // while(((uint32_t)types & 0x03) && ++types < end);
    data = types+calculateTypeTagLength();
    // while(*data == '/0') && (data & 0x03) && ++data < end);
    // while(((uint32_t)data & 0x03) && ++data < end);
  }

  char* getAddress(){
    return (char*)address;
  }

  bool matches(const char* pattern){
    return matches(pattern, strlen(pattern));
  }

  bool matches(const char* pattern, int length){
    // return strncmp(address, pattern, length) == 0;
    // todo: wild cards?
    return strncasecmp((char*)address, pattern, length) == 0;
  }
 
  /*
   * @return number of arguments (data fields) in the message
   */
  uint8_t getSize(){
    uint8_t size = 0;
    for(int i=1; types && types[i] != '\0' && &types[i] < data; ++i)
      size++;
    return size;
  }

  char getDataType(uint8_t index){
    return types[index+1]; // the first character is a comma
  }

  int getDataSize(uint8_t index){
    char type = getDataType(index);
    switch(type){
    case 'c': // ASCII character sent as 32 bits
    case 'r': // 32bit RGBA colour
    case 'i': // 32bit integer
    case 'f': // 32bit float
    case 'm': // 4-byte MIDI message
      return 4;
    case 'h': // 64bit integer
    case 'd': // 64bit double
      return 8;
    case 'b': // blob
      // first four bytes is the size
      return *(uint32_t*)getData(index);
    case 's': // string
    case 'S': // symbol
      {
	char* str = (char*)getData(index);
	int8_t size = strnlen(str, (char*)end-str)+1;
	while(size & 0x03)
	  size++;
	return size;
      }
    case 'T': // TRUE
    case 'F': // FALSE
    case 'N': // NIL
    case 'I': // INFINITUM
    default:
      return 0;
    }
  }

  float getAsFloat(uint8_t i){
    float value;
    switch(getDataType(i)){
    case 'f':
      value = getFloat(i);
      break;
    case 'i':
      value = float(getInt(i));
      break;
    case 'd':
      value = float(getDouble(i));
      break;
    case 'h':
      value = float(getLong(i));
      break;
    case 'T':
      value = 1.0f;
      break;
    default:
      value = 0.0f;
      break;
    }
    return value;
  }

  float getAsInt(uint8_t i){
    int value;
    switch(getDataType(i)){
    case 'f':
      value = int(getFloat(i));
      break;
    case 'i':
      value = getInt(i);
      break;
    case 'd':
      value = int(getDouble(i));
      break;
    case 'h':
      value = int(getLong(i));
      break;
    case 'T':
      value = 1;
      break;
    default:
      value = 0;
      break;
    }
    return value;
  }

  bool getAsBool(uint8_t i){
    bool value;
    switch(getDataType(i)){
    case 'f':
      value = getFloat(i) > 0.5;
      break;
    case 'i':
      value = getInt(i) != 0;
      break;
    case 'd':
      value = getDouble(i) > 0.5;
      break;
    case 'h':
      value = getLong(i) != 0;
      break;
    case 'T':
      value = true;
      break;
    default:
      value = false;
      break;
    }
    return value;
  }

  void* getData(uint8_t index){
    uint8_t* ptr = data;
    for(int i=0; i<index; ++i)
      ptr += getDataSize(i);
    return ptr;
  }

  int32_t getInt(int8_t index){
    return getOscInt32((uint8_t*)getData(index));
  }

  float getFloat(int8_t index){
    return getOscFloat32((uint8_t*)getData(index));
  }

  int64_t getLong(int8_t index){
    return getOscInt64((uint8_t*)getData(index));
  }

  double getDouble(int8_t index){
    return getOscFloat64((uint8_t*)getData(index));
  }

  char* getString(int8_t index){
    return (char*)getData(index);
  }

  static int32_t getOscInt32(uint8_t* data){
    union { int32_t i; uint8_t b[4]; } u;
    u.b[3] = data[0];
    u.b[2] = data[1];
    u.b[1] = data[2];
    u.b[0] = data[3];
    return u.i;
  }

  static float getOscFloat32(uint8_t* data){
    union { float f; uint8_t b[4]; } u;
    u.b[3] = data[0];
    u.b[2] = data[1];
    u.b[1] = data[2];
    u.b[0] = data[3];
    return u.f;
  }

  static int64_t getOscInt64(uint8_t* data){
    union { int64_t i; uint8_t b[8]; } u;
    u.b[7] = data[0];
    u.b[6] = data[1];
    u.b[5] = data[2];
    u.b[4] = data[3];
    u.b[3] = data[4];
    u.b[2] = data[5];
    u.b[1] = data[6];
    u.b[0] = data[7];
    return u.i;
  }

  static float getOscFloat64(uint8_t* data){
    union { double d; uint8_t b[8]; } u;
    u.b[7] = data[0];
    u.b[6] = data[1];
    u.b[5] = data[2];
    u.b[4] = data[3];
    u.b[3] = data[4];
    u.b[2] = data[5];
    u.b[1] = data[6];
    u.b[0] = data[7];
    return u.d;
  }

  static int writeOscString(void* dest, const char* str){
    int len = strlen(str);
    while(++len & 0x03); // pad to word boundary
    stpncpy((char*)dest, str, len);
    return len;
  }

  void setAddress(const char* addr){
    if(getPrefixLength() > 0 && types != address){
      // save old types
      int len = getPrefixLength();
      char saved[len];
      stpncpy(saved, (char*)types, len);
      types = address + writeOscString(address, addr);
      data = types + writeOscString(types, saved);
    }else{
      types = address + writeOscString(address, addr);
      data = types;
    }
  }

  void setPrefix(const char* addr, const char* tags){
    types = address + writeOscString(address, addr);
    data = types + writeOscString(types, tags);
  }

  // void write(Print& out){
  //   out.write(address, calculateMessageLength());
  // }

  void setData(int index, void* data, int size){
    void* ptr = getData(index);
    memcpy(ptr, data, size);
  }

  void setFloat(uint8_t index, float value){
    set32(index, (uint8_t*)&value);
  }

  /* get a T or F (true or false) from the type tags */
  bool getBool(uint8_t index){
    return types[index+1] == 'T';
  }

  /* set a T or F (true or false) in the type tags */
  void setBool(uint8_t index, bool value){
    if(value)
      types[index+1] = 'T';
    else
      types[index+1] = 'F';
  }

  void setInt(uint8_t index, int32_t value){
    set32(index, (uint8_t*)&value);
  }

  void setDouble(uint8_t index, double value){
    set64(index, (uint8_t*)&value);
  }

  void setLong(uint8_t index, int64_t value){
    set64(index, (uint8_t*)&value);
  }

  void setString(uint8_t index, const char* str){
    setString(index, str, strlen(str));
  }

  void setString(uint8_t index, const char* str, size_t length){
    char* ptr = (char*)getData(index);
    for(size_t i=0; i<length && ptr < (char*)end-1; ++i)
      *ptr++ = str[i];
    do{
      // pad to 4 bytes
      *ptr++ = '\0';
    }while(++length & 0x03);
  }

protected:
  void set32(uint8_t index, uint8_t* value){
    uint8_t* ptr = (uint8_t*)getData(index);
    *ptr++ = value[3];
    *ptr++ = value[2];
    *ptr++ = value[1];
    *ptr++ = value[0];
  }

  void set64(uint8_t index, uint8_t* value){
    uint8_t* ptr = (uint8_t*)getData(index);
    *ptr++ = value[7];
    *ptr++ = value[6];
    *ptr++ = value[5];
    *ptr++ = value[4];
    *ptr++ = value[3];
    *ptr++ = value[2];
    *ptr++ = value[1];
    *ptr++ = value[0];
  }
};

#endif /*  __OscMessage_h__ */
