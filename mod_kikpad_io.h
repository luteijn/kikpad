/*
__ __| |           |  /_) |     ___|             |           |
   |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
   |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
  _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
  -----------------------------------------------------------------------------
  KIKPAD  - Alternative firmware for the Midiplus Smartpad.
  Copyright (C) 2020 by The KikGen labs.
  LICENCE CREATIVE COMMONS - Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)

  This file is part of the KIKPAD distribution
  https://github.com/TheKikGen/kikpad
  Copyright (c) 2020 TheKikGen Labs team.
  -----------------------------------------------------------------------------
  Disclaimer.

  This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/
  or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

  NON COMMERCIAL - PERSONAL USE ONLY : You may not use the material for pure
  commercial closed code solution without the licensor permission.

  You are free to copy and redistribute the material in any medium or format,
  adapt, transform, and build upon the material.

  You must give appropriate credit, a link to the github site
  https://github.com/TheKikGen/USBMidiKliK4x4 , provide a link to the license,
  and indicate if changes were made. You may do so in any reasonable manner,
  but not in any way that suggests the licensor endorses you or your use.

  You may not apply legal terms or technological measures that legally restrict
  others from doing anything the license permits.

  You do not have to comply with the license for elements of the material
  in the public domain or where your use is permitted by an applicable exception
  or limitation.

  No warranties are given. The license may not give you all of the permissions
  necessary for your intended use.  This program is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

// KIKPAD_IO : A Midi in/out dumb keyboard

#ifndef _KIKPAD_MODULE_H_
#define _KIKPAD_MODULE_H_

#define MY_CHANNEL (0x0)

void dumpbyte(uint8_t val, uint8_t row) {
  uint8_t i;
  uint8_t mask=0x80;
  row=row*8;
  for (i=0;i<8;i++){
    PadSetColor(i+row,val&mask?RED:BLUE);
    mask=mask>>1;
  }
}

uint8_t encoder_state(uint8_t idx,uint8_t val) {
  static uint8_t encoderVal[8] = {0,0,0,0,0,0,0,0};
  
  if (idx<8) {
    if (val<128) encoderVal[idx]=val;
    return encoderVal[idx] ;
  }
  return 0 ;
}

///////////////////////////////////////////////////////////////////////////////
// PARSE A RECEIVED USB MIDI PACKET
///////////////////////////////////////////////////////////////////////////////
void KikpadMod_USBMidiParse(midiPacket_t *pk)
{  
  // pk->packet[0] is the TYPE of message, often just [1]>>4, [1] has the midi status byte, rest the data bytes.
  uint8_t channel = (pk->packet[1] & 0x0F) ;
  uint8_t type = (pk->packet[1] & 0xF0) ;
  
  if ( channel == MY_CHANNEL ) {
      switch (type) {
          case 0x80: // note off
            if (pk->packet[2]<64) {
              PadSetColor( pk->packet[2], BLACK);
            } else {
              ButtonSetLed( pk->packet[2]-64, OFF);
            }
            break;
          case 0x90: // note on
          case 0xA0: // PA
            if (pk->packet[2]<64) {
              PadSetColor( pk->packet[2], pk->packet[3]);  
            } else {
              ButtonSetLed( pk->packet[2]-64, pk->packet[3]?ON:OFF); 
            }
            break;
             
          case 0xB0: // CC event
            encoder_state(pk->packet[2],pk->packet[3]); // update setting of this controller
            break;  
          case 0xC0: // PC event
            // TODO: do something interesting with this, e.g. update default velocity, change channel we listen on, clear something?
            break;  
          default: // unhandled event, dump first four bytes FIXME: dump the exact number of bytes based on the length given in the table in usb_midi
            dumpbyte(pk->packet[0],0);
            dumpbyte(pk->packet[1],1);
            dumpbyte(pk->packet[2],2);
            dumpbyte(pk->packet[3],3);
  
            break;
      }
  } else {
  }
  return;
}

///////////////////////////////////////////////////////////////////////////////
// PARSE A RECEIVED USER EVENT
///////////////////////////////////////////////////////////////////////////////
void KikpadMod_ProcessUserEvent(UserEvent_t *ev){
  static uint8_t midiChannel = MY_CHANNEL;
  uint8_t encoderVal; 
  midiPacket_t pk2;

  uint8_t idx = SCAN_IDX(ev->d1,ev->d2);

  switch (ev->ev) {

    // Encoders Clock wise
    case EV_EC_CW:
      encoderVal=encoder_state(idx,128); //get
      if ( ++encoderVal > 127 ) encoderVal = 127; //update
      encoder_state(idx,encoderVal); //set
      pk2.packet[0] = 0x0B;
      pk2.packet[1] = 0xB0 + midiChannel;
      pk2.packet[2] = idx;
      pk2.packet[3] = encoderVal;
      MidiUSB.writePacket(&pk2.i);

      break;

    // Encoders Counter Clock wise
    case EV_EC_CCW:
      encoderVal=encoder_state(idx,128); //get
      if ( encoderVal > 0 ) encoderVal--; //update
      encoder_state(idx,encoderVal); //set
      pk2.packet[0] = 0x0B;
      pk2.packet[1] = 0xB0 + midiChannel;
      pk2.packet[2] = idx;
      pk2.packet[3] = encoderVal;
      MidiUSB.writePacket(&pk2.i);


      break;

    // Pad pressed and not released
    case EV_PAD_PRESSED:
          // Send Note On
          pk2.packet[0] = 0x09;
          pk2.packet[1] = 0x90 + midiChannel;
          pk2.packet[2] = idx;
          pk2.packet[3] = 0x7F;
          MidiUSB.writePacket(&pk2.i);
      break;

    // Pad released
    case EV_PAD_RELEASED:
          // Send Note Off
          pk2.packet[0] = 0x08;
          pk2.packet[1] = 0x80 + midiChannel;
          pk2.packet[2] = idx;
          pk2.packet[3] = 0x40;
          MidiUSB.writePacket(&pk2.i);
        
      break;

    // Button pressed and not released
    case EV_BTN_PRESSED:
          // Send Note On
          pk2.packet[0] = 0x09;
          pk2.packet[1] = 0x90 + midiChannel;
          pk2.packet[2] = idx+64;
          pk2.packet[3] = 0x7F;
          MidiUSB.writePacket(&pk2.i);
      break;

    // Button released
    case EV_BTN_RELEASED:
          // Send Note Off
          pk2.packet[0] = 0x08;
          pk2.packet[1] = 0x80 + midiChannel;
          pk2.packet[2] = idx+64;
          pk2.packet[3] = 0x40;
          MidiUSB.writePacket(&pk2.i);
       break;

    // Button pressed and held more than 2s
    case EV_BTN_HELD:

      break;

  }

}

///////////////////////////////////////////////////////////////////////////////
// Kikpad module setup
///////////////////////////////////////////////////////////////////////////////
static void KikpadMod_Setup() {
  int i;
  PadLedStates[0] = PadLedStates[1] = 0XFFFFFFFF ;
  for (i=0;i<8;i++){
    encoder_state(i,63);
  }
  for (i=0;i<64;i++){
    PadSetColor(i,i);
  }
  delay(500); 
  for (i=0;i<64;i++){
    PadSetColor(i,BLACK); 
  }
}
///////////////////////////////////////////////////////////////////////////////
// Kikpad module loop
///////////////////////////////////////////////////////////////////////////////
static void KikpadMod_Loop() {

}


#endif
