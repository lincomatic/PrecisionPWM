// 20170116 SCL copyright 2017 Sam C. Lin
// hacked from https://blog.blinkenlight.net/experiments/measurements/flexible-sweep/
//
//  www.blinkenlight.net
//
//  Copyright 2012 Udo Klein
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see http://www.gnu.org/licenses/
 
uint32_t power_of_ten(uint8_t exponent) {
    uint32_t result = 1;
     
    while (exponent > 0) {
        --exponent;
        result *= 10;
    }
    return result;
}
 
// can't use typedef because Arduino sucks
#define fixed_point uint32_t
 
// notice 2^32 = 4 294 967 296 ~ 4*10^9
// 10^3*10^4*phases = 1.6 * 10^8 ==> fits into uint32_t
// ==> At most one more digit might be possible but we will not push this to the limits.
//     It is somewhat pointless anyway because we are right now at the edge of reasonable
//     precision.
 
#define PWM_PIN 13
static uint8_t duty_cycle = 2; // duty cycle = 1/duty
const uint8_t max_integer_digits    = 4;
const uint8_t max_fractional_digits = 4;
// constant value for the number 1 in a fixed point represenation with
// max_fractional_digits behind the dot
const fixed_point fixed_point_1 = (fixed_point)power_of_ten(max_fractional_digits);

const uint64_t system_clock_frequency = ((uint64_t)F_CPU) * fixed_point_1;
const fixed_point default_frequency = 7 * fixed_point_1;
 
const uint16_t delay_block_tick_size = 30000; // must be below 2^15-1 in order for tail ticks to fit into uint16_t
// total delay time will be delay_block_tick_size * delay_blocks + tail_ticks + nominator/denominator
volatile uint32_t delay_blocks = 0;
volatile uint16_t tail_ticks   = 1000;  // will always be <= 2*delay_block_tick_size
volatile fixed_point denominator = 1;
volatile fixed_point nominator = 0;
volatile boolean reset_timer = true;
 
 
inline char get_next_available_character() {
    while (!Serial.available()) {}
     
    return Serial.read();
}
 
fixed_point parse()
{
  const char decimal_separator     = '.';
  const char terminator[] = " \t\n\r";
  
  enum parser_state { parse_integer_part                = 0,
		      parse_fractional_part             = 1,
		      error_duplicate_decimal_separator = 2, 
		      error_invalid_character           = 3,
		      error_missing_input               = 4,
		      error_to_many_decimal_digits      = 5,
		      error_to_many_fractional_digits   = 6 };
  
  while (true) {
    
    fixed_point value = 0;
    uint8_t parsed_digits = 0;        
    parser_state state = parse_integer_part;
    
    while (state == parse_integer_part || state == parse_fractional_part) {
      
      const char c = get_next_available_character();        
      if (c == decimal_separator) {
	if (state == parse_integer_part) { 
	  state = parse_fractional_part;
	  parsed_digits = 0;
	} else {
	  state = error_duplicate_decimal_separator;
	}
      } else            
	if (strchr(terminator, c) != NULL) {
	  if (state == parse_integer_part && parsed_digits == 0) {
	    state = error_missing_input;
	  } else {
	    return value * (fixed_point)power_of_ten(max_fractional_digits - (state == parse_integer_part? 0: parsed_digits));                                                     
	  }
	} else           
	  if (c >= '0' and c <= '9') {
	    ++parsed_digits;
	    value = value * 10 + c - '0';                                     
	    if (state == parse_integer_part && parsed_digits > max_integer_digits) {
	      state = error_to_many_decimal_digits;
	    } else
	      if (state == parse_fractional_part && parsed_digits > max_fractional_digits) {
		state = error_to_many_fractional_digits;
	      }                                
	  } else {
	    state = error_invalid_character;
	  }        
    }
    Serial.print(F("Error: "));
    Serial.println(state);
  }    
}

 
 
void set_timer_cycles(uint16_t cycles) {    
    OCR1A = cycles - 1;
}
 
ISR(TIMER1_COMPA_vect) {
    // To decrease phase jitter "next_phase" is always precomputed.
    // Thus the start of the ISR till the manipulation of the IO pins 
    // will always take the same amount of time.
        
    static uint32_t blocks_to_delay;
    static fixed_point accumulated_fractional_ticks;
 
    if (reset_timer) {
        reset_timer = false;
         
        blocks_to_delay = 0;
        accumulated_fractional_ticks = 0;
    }
 
    if (blocks_to_delay == 0) {    
      static uint8_t duty = 0;
      if (duty == 0) digitalWrite(PWM_PIN,HIGH);
      else if (duty == 1) digitalWrite(PWM_PIN,LOW);
      if (++duty == duty_cycle) duty = 0;
      
        blocks_to_delay = delay_blocks;
        if (blocks_to_delay > 0) {
            set_timer_cycles(delay_block_tick_size);          
        }        
    } else {
        --blocks_to_delay;
    }
         
    if (blocks_to_delay == 0) {
        accumulated_fractional_ticks += nominator;
        if (accumulated_fractional_ticks < denominator) {                
            set_timer_cycles(tail_ticks);
        } else {
            set_timer_cycles(tail_ticks+1);
            accumulated_fractional_ticks -= denominator;
        }              
    }     
}
 
void set_target_frequency(fixed_point target_frequency)
{
  uint32_t tf = target_frequency;
  target_frequency *= duty_cycle;
    // total delay time will be delay_block_tick_size * delay_blocks + tail_ticks + nominator/denominator  
    cli();
 
    // compute the integer part of the period length "delay_ticks" as well as its fractional part "nominator/denominator"
    denominator = target_frequency;       
    uint64_t delay_ticks = system_clock_frequency / denominator;    
    nominator = system_clock_frequency - delay_ticks * denominator;
  
    // break down delay_ticks in chunks that can be handled with a 16 bit timer   
    delay_blocks = delay_ticks / delay_block_tick_size;
    tail_ticks   = delay_ticks - delay_block_tick_size * delay_blocks;
    if (delay_blocks > 0) {
        // enforce that tails are always longer than 1000 ticks
        --delay_blocks;
        tail_ticks += delay_block_tick_size;
    }
         
    // tell the timer ISR to reset its internal values
    reset_timer = true;
 
    sei();    
     
    Serial.print(F("target frequency: "));
    uint32_t mult = power_of_ten(max_fractional_digits);
    Serial.print(tf / mult);
    Serial.print(".");
    Serial.println(tf % mult);
    
    /*
    // debugging only
    Serial.print(F("denominator: ")); Serial.println(denominator); 
    Serial.print(F("nominator: ")); Serial.println(nominator); 
    Serial.print(F("delay ticks: ")); Serial.print((uint32_t)delay_ticks); 
    Serial.print(','); Serial.println((uint32_t)(delay_ticks>>32)); 
    Serial.print(F("delay blocks: ")); Serial.println(delay_blocks);     
    Serial.print(F("tail ticks: ")); Serial.println(tail_ticks);     
    */
};
 
 
void setup()
{
  Serial.begin(115200);
  pinMode(PWM_PIN,OUTPUT);
  
  // disable timer0 interrupts to stop Arduino timer functions
  // from introducing jitter in our output
  TIMSK0 = 0;
  
  // disable timer1 interrupts
  TIMSK1 = 0;
  
  // do not toggle or change timer IO pins
  TCCR1A = 0;
  // Mode 4, CTC using OCR1A | set prescaler to 1
  TCCR1B = (1<<WGM12) | (1<<CS10);
  
  Serial.print("duty cycle: 1/");Serial.println(duty_cycle);
  set_target_frequency(default_frequency);
  
  // enable match interrupts
  TIMSK1 = 1<<OCIE1A;    
}
 
void loop() { 
    set_target_frequency(parse());    
}
