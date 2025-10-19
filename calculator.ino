#include <LiquidCrystal_I2C.h>

constexpr uint8_t i2c_port = 0x27;

constexpr uint8_t button_a_pin = 7;
constexpr uint8_t button_b_pin = 6;

constexpr uint8_t screen_size_x = 16;
constexpr uint8_t screen_size_y = 2;

static_assert((screen_size_x > 0) && (screen_size_y > 0),
  "Not valid display size.");

constexpr uint8_t input_buf_size =
  (screen_size_y == 1) ? (screen_size_x - 2) : (screen_size_x);

static_assert(input_buf_size >= 3,
  "Diplay is too small.");

LiquidCrystal_I2C lcd(i2c_port, screen_size_x, screen_size_y);

bool error_flag = false;

char input_buf[input_buf_size + 1] = {'\0'};

uint8_t cursor_pos = 0;

char options[] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '(', ')', '+', '-', '*', '/', '%', '=', '<'
};

uint8_t option_index = 0;

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

void print_char(char c) {
  if (cursor_pos < input_buf_size) {
    input_buf[cursor_pos] = c;
    lcd.write(c);
    cursor_pos++;
  }
}

void delete_last_char() {
  if (cursor_pos > 0) {
    cursor_pos--;
    input_buf[cursor_pos] = '\0';
    lcd.setCursor(cursor_pos, 0);
    lcd.write(' ');
    lcd.setCursor(cursor_pos, 0);
  }
}

void clear() {
  cursor_pos = 0;
  lcd.setCursor(0, 0);
  for (uint8_t i = 0; i < input_buf_size; i++) {
    input_buf[i] = '\0';
    lcd.write(' ');
  }
  lcd.setCursor(0, 0);
}

void print_int16(int16_t value) {
  if (value == 0) {
    print_char('0');
    return;
  }

  char buf[6] = {0};

  uint32_t abs_value = abs(value);
  uint8_t index = 0;

  while (abs_value > 0) {
    buf[index] = abs_value % 10 + '0';
    abs_value /= 10;
    index++;
  }

  if (value < 0) {
    buf[index] = '-';
    index++;
  }

  for (uint8_t j = 0; j < index; j++) {
    print_char(buf[index - 1 - j]);
  }
}

int16_t str_to_int16(uint8_t start, uint8_t size) {
  if (size == 0) {
    error_flag = true;
    return 0;
  }

  int32_t value = 0;
  uint8_t index = 0;

  while (index < size) {
    if (!is_digit(input_buf[start + index])) {
      error_flag = true;
      return 0;
    }

    value *= 10;
    value += input_buf[start + index] - '0';

    if (value > 32767) {
      error_flag = true;
      return 0;
    }

    index++;
  }

  if (index == 0)
    error_flag = true;

  return (int16_t)value;
}

void print_option() {
  lcd.setCursor(screen_size_x - 1, screen_size_y - 1);
  lcd.write(options[option_index]);
  lcd.setCursor(cursor_pos, 0);
}

char get_current_char(uint8_t *current) {
  return input_buf[*current];
}

void advance(uint8_t *current) {
  *current = min((*current) + 1, input_buf_size);
}

int16_t calculate_expression(uint8_t *current);

int16_t calculate_factor(uint8_t *current) {
  if (get_current_char(current) == '(') {
    advance(current);

    int16_t result = calculate_expression(current);

    if (error_flag)
      return 0;
    
    if (get_current_char(current) != ')') {
      error_flag = true;
      return 0;
    }

    advance(current);

    return result;
  } else if (get_current_char(current) == '-') {
    advance(current);

    int16_t result = calculate_factor(current);

    if (error_flag)
      return 0;

    return -result;
  } else if (get_current_char(current) == '+') {
    advance(current);

    int16_t result = calculate_factor(current);

    if (error_flag)
      return 0;

    return result;
  } else if (is_digit(get_current_char(current))) {
    uint8_t start = *current;

    while (is_digit(get_current_char(current)))
      advance(current);
    
    int16_t result = str_to_int16(start, (*current) - start);

    if (error_flag)
      return 0;

    return result;
  }

  error_flag = true;
  return 0;
}

int16_t calculate_term(uint8_t *current) {
  int16_t result = calculate_factor(current);

  if (error_flag)
    return 0;
  
  while (get_current_char(current) == '*' || get_current_char(current) == '/'
    || get_current_char(current) == '%') {
    switch (get_current_char(current)) {
      case '*': {
        advance(current);

        int16_t b = calculate_factor(current);

        if (error_flag)
          return 0;

        result *= b;
        break;
      }
      case '/': {
        advance(current);

        int16_t b = calculate_factor(current);

        if (error_flag)
          return 0;

        if (b == 0) {
          error_flag = true;
          return 0;
        }

        result /= b;
        break;
      }
      case '%': {
        advance(current);

        int16_t b = calculate_factor(current);

        if (error_flag)
          return 0;

        if (b == 0) {
          error_flag = true;
          return 0;
        }

        result %= b;
        break;
      }
    }
  }

  return result;
}

int16_t calculate_expression(uint8_t *current) {
  int16_t result = calculate_term(current);

  if (error_flag)
    return 0;
  
  while (get_current_char(current) == '+' || get_current_char(current) == '-') {
    switch (get_current_char(current)) {
      case '+': {
        advance(current);

        int16_t b = calculate_term(current);

        if (error_flag)
          return 0;

        result += b;
        break;
      }
      case '-': {
        advance(current);

        int16_t b = calculate_term(current);

        if (error_flag)
          return 0;

        result -= b;
        break;
      }
    }
  }

  return result;
}

void calculate() {
  uint8_t current = 0;
  int16_t result = calculate_expression(&current);

  if (input_buf[current] != '\0')
    error_flag = true;

  clear();

  if (error_flag) {
    lcd.print("error");
    return;
  }

  print_int16(result);
}

void use_option() {
  char option = options[option_index];
  switch (option) {
    case '<':
      delete_last_char();
      break;
    case '=':
      calculate();
      break;
    default:
      print_char(option);
      break;
  }
}

void setup() {
  pinMode(button_a_pin, INPUT);
  pinMode(button_b_pin, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.cursor();

  print_option();
}

void loop() {
  if (digitalRead(button_a_pin)) {
    if (error_flag) {
      clear();
      error_flag = false;
    } else {
      option_index = (option_index + 1) % (sizeof(options) / sizeof(char));
      print_option();
    }
    delay(250);
  }

  if (digitalRead(button_b_pin)) {
    if (error_flag) {
      clear();
      error_flag = false;
    } else {
      use_option();
    }
    delay(250);
  }
}
