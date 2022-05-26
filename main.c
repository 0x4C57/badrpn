// SPDX-License-Identifier: MIT

// BadRpn - Crude command line RPN calculator that does fixed point arithmatic and only +-X÷

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#if defined(__APPLE__) || defined(__unix__)
  #define UNIX_CONSOLE 1
#else
  #define UNIX_CONSOLE 0
#endif
#define FIXED_POINT_PERCISION 3
#define STACK_SIZE 256

// Stack pointer always points to an empty stack cell
int rpn_stack[STACK_SIZE], rpn_stk_index = 0;

// If error occured, this flag is set and after the operation has been taken
// the stack would be reset and ans would be the only value on stack.
bool reset_status = false;

struct termios def_info;

typedef long num_t;

double disp_divider = 1.0;
num_t  calc_divider = 1;

// Answer of last calculation. Its value is given when an operation is input but
// a new number has not yet been pushed onto the stack.
// It's mainly, or only used when it's needed to restore the last answer
// when stack error happens.
num_t ans = 0;

typedef enum _calc_oper
{
  none = 0,
  plus, minus, multiply, divide,
  quit, // Quit app
  ac, // Clear All
  push = 0x80000000
} calc_oper;

typedef struct _input_event
{
    num_t input_number;
    calc_oper oper;
} input_event;

void rpn_reset(num_t initial)
{
  rpn_stk_index = initial == 0 ? 0 : 1;
  rpn_stack[0] = initial;
}

void rpn_push(num_t num)
{
  if(rpn_stk_index == STACK_SIZE)
  {
    puts("\nERROR: STACK OVERFLOW!");
    reset_status = true;
    return;
  }
  rpn_stack[rpn_stk_index++] = num;
}

num_t rpn_pop()
{
  if(rpn_stk_index == 0)
  {
    puts("\nERROR: STACK UNDERFLOW!");
    reset_status = true;
    return 1;
  }
  return rpn_stack[--rpn_stk_index];
}

num_t rpn_peek()
{
  if(rpn_stk_index == 0)
    return 0;
  return rpn_stack[rpn_stk_index - 1];
}

input_event get_input()
{
  input_event ret;
  bool reading_decimal = false;
  int decimal_place_length = 0, input_length = 0, i;
  char c;
  ret.input_number = 0;
  ret.oper = none;
  while(1)
  {
    c = getchar();

    if(c >= '0' && c <= '9') // Numeric
    {
      if(!reading_decimal ||
         (reading_decimal && decimal_place_length < FIXED_POINT_PERCISION))
      {
        // Only add to the number when not yet after decimal point,
        // or when the input hasn't exceeded decimal place limitation
        // If exceeded the fixed point limit, the input will be discarded
        ret.input_number *= 10;
        ret.input_number += c - '0';
        if(reading_decimal)
          decimal_place_length++;
        ret.oper |= push;

        putchar(c); // Echo
        input_length++;
      }
      continue;
    }

    switch(c) // Operators
    {
      case '.' :
        // Enter decimal mode
        if(!reading_decimal)
        {
          putchar(c);
          input_length++;
        }
        reading_decimal = true;
        break;
      case ' ' :
      case '\n': ret.oper |= push; goto commit_input; // Push onto stack
      case '_' : ret.input_number *= -1; putchar(c); input_length++; break; // Negate
      case '+' : ret.oper |= plus; goto commit_input;
      case '-' : ret.oper |= minus; goto commit_input;
      case '*' : ret.oper |= multiply; goto commit_input;
      case '/' : ret.oper |= divide; goto commit_input;
      case ';' : ret.oper =  ac; goto commit_input;
      case 0x1B: /* ASCII ESC */ ret.oper |= quit; goto commit_input;
      case 0x7F: // ASCII DEL
        // Backspace is a bit more complicated
        ret.input_number /= 10;
        if(input_length-- > 0)
          printf("\b \b");
        if(reading_decimal)
        {
          if(decimal_place_length > 0)
            decimal_place_length--;
          else
            reading_decimal = false;
        }
        break;

//      default:
//        printf("[[%2x]]", c);
    }
  }

commit_input:
  if(input_length == 0 && !(ret.oper & ~push)) // No operation, no number
    ret.oper = none;
  else if(c != '\n') // Print the operator
    putchar(c);

  // Add trailing zeros to fixed point number
  for(i = 0; i < FIXED_POINT_PERCISION - decimal_place_length; i++)
    ret.input_number *= 10;
  return ret;
}

int rpn_proc()
{
  while(1)
  {
    printf("[%d] > ", rpn_stk_index);
    input_event input = get_input();

    if(input.oper == quit)
      return 0;
    else if(input.oper == ac)
    {
      rpn_reset(0);
      putchar('\n');
      continue;
    }

    if(input.oper & ~push)
      ans = rpn_pop();
//    printf("<%8x>\n", input.oper);
    if(input.oper & push)
      rpn_push(input.input_number);
    input.oper &= ~push;
    switch(input.oper)
    {
      case plus:
        rpn_push(ans + rpn_pop());
        break;

      case minus:
        rpn_push(ans - rpn_pop());
        break;

      case multiply:
        rpn_push(ans * rpn_pop() / calc_divider);
        break;

      case divide:
      {
        rpn_push(ans * calc_divider / rpn_pop());
      }
      default: break;
    }

    if(reset_status)
      rpn_reset(ans), reset_status = false;

    printf(" >>> %.3f\n", rpn_peek() / disp_divider);
  }
}

void cleanup()
{
  tcsetattr(0, TCSANOW, &def_info);
}

int main()
{
  if(UNIX_CONSOLE)
  {
    struct termios info;
    tcgetattr(0, &def_info);           /* get current terminal attirbutes; 0 is the file descriptor for stdin */
    info = def_info;                   /* make a copy */
    info.c_lflag &= ~ICANON & ~ECHO;   /* disable canonical mode and echo */
    info.c_cc[VMIN] = 1;               /* wait until at least one keystroke available */
    info.c_cc[VTIME] = 0;              /* no timeout */
    tcsetattr(0, TCSANOW, &info);      /* set immediately */

    atexit(cleanup);
  }
  for(int i = 0; i < FIXED_POINT_PERCISION; i++)
    disp_divider *= 10, calc_divider *= 10;
  rpn_proc();
  return 0;
}
