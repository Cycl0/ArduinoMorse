#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIN_BUZ 13
#define PIN_BTN_REPLAY 12
#define PIN_BTN_MORSE_TO_TXT 11
#define PIN_BTN_ERASE 10

#define ERASE_FREQ 100
#define MORSE_FREQ 600
#define DURATION_DOT_SOUND 10
#define DURATION_DASH_SOUND 100
#define DELAY_SOUND 100
#define DELAY_BETWEEN_LETTERS_SOUND 300
#define MORSE_LENGTH 5

#define IDLE_STATE 0
#define DOT_STATE 1
#define DASH_STATE 2
#define PRINT_STATE 3

#define DELAY_DEBOUNCE 10
#define DELAY_DASH_INPUT 200
#define DELAY_BETWEEN_INPUT 1000

#define OFFSET_DIGIT_ASCII 22
#define OFFSET_LETTER_ASCII 97

#define LCD_MORSE_ADDR 0x27
#define LCD_TEXT_ADDR 0x3F

#define LINE_LENGTH 20
#define NUMBER_LINES 4
#define LCD_SIZE  80
#define CURSOR_BLINK_DELAY 200

typedef uint8_t byte;

// Posicao do cursor
typedef struct {
  byte x;
  byte y;
  unsigned long lastCursorBlink;
  char cursorSymbol;
} Cursor;

// Codigo morse como array
typedef struct {
  byte i;
  byte code[MORSE_LENGTH];
} Morse;

// Setup de LCD
void LCD_Setup(LiquidCrystal_I2C* lcd) {
  lcd->init();
  lcd->backlight();
  lcd->setCursor(0, 0);
}

// LCD do morse
LiquidCrystal_I2C lcdMorse(LCD_MORSE_ADDR, LINE_LENGTH, NUMBER_LINES);
Cursor cursorMorse = {.x=0, .y=0, .lastCursorBlink=0, .cursorSymbol = '|'};

// LCD do texto
LiquidCrystal_I2C lcdText(LCD_TEXT_ADDR, LINE_LENGTH, NUMBER_LINES);
Cursor cursorText = {.x=0, .y=0, .lastCursorBlink=0, .cursorSymbol = '|'};

Morse morse = {.i=0};

void setup() {
  // Declara modo dos pinos
  pinMode(PIN_BUZ, OUTPUT);
  pinMode(PIN_BTN_REPLAY, INPUT_PULLUP);
  pinMode(PIN_BTN_MORSE_TO_TXT, INPUT_PULLUP);
  pinMode(PIN_BTN_ERASE, INPUT_PULLUP);
  LCD_Setup(&lcdMorse); // Setup do LCD do morse
  LCD_Setup(&lcdText); // Setup do LCD do texto
  Serial.begin(115200);
}

byte morseDictionary[][MORSE_LENGTH + 1] = { // Código com tamanho no primeiro index
  // 0 é ·
  // 1 é −
  {2, 0, 1}, // Código pra a
  {4, 1, 0, 0, 0}, // Código pra b
  {4, 1, 0, 1, 0}, // Código pra c
  {3, 1, 0, 0}, // Código pra d
  {1, 0}, // Código pra e
  {4, 0, 0, 1, 0}, // Código pra f
  {3, 1, 1, 0}, // Código pra g
  {4, 0, 0, 0, 0}, // Código pra h
  {2, 0, 0}, // Código pra i
  {4, 0, 1, 1, 1}, // Código pra j
  {3, 1, 0, 1}, // Código pra k
  {4, 0, 1, 0, 0}, // Código pra l
  {2, 1, 1}, // Código pra m
  {2, 1, 0}, // Código pra n
  {3, 1, 1, 1}, // Código pra o
  {4, 0, 1, 1, 0}, // Código pra p
  {4, 1, 1, 0, 1}, // Código pra q
  {3, 0, 1, 0}, // Código pra r
  {3, 0, 0, 0}, // Código pra s
  {1, 1}, // Código pra t
  {3, 0, 0, 1}, // Código pra u
  {4, 0, 0, 0, 1}, // Código pra v
  {3, 0, 1, 1}, // Código pra w
  {4, 1, 0, 0, 1}, // Código pra x
  {4, 1, 0, 1, 1}, // Código pra y
  {4, 1, 1, 0, 0}, // Código pra z
  {5, 1, 1, 1, 1, 1}, // Código pra 0
  {5, 0, 1, 1, 1, 1}, // Código pra 1
  {5, 0, 0, 1, 1, 1}, // Código pra 2
  {5, 0, 0, 0, 1, 1}, // Código pra 3
  {5, 0, 0, 0, 0, 1}, // Código pra 4
  {5, 0, 0, 0, 0, 0}, // Código pra 5
  {5, 1, 0, 0, 0, 0}, // Código pra 6
  {5, 1, 1, 0, 0, 0}, // Código pra 7
  {5, 1, 1, 1, 0, 0}, // Código pra 8
  {5, 1, 1, 1, 1, 0} // Código pra 9
};

String entireMorseStr = "";
String oneScreenMorseStr = "";
String entireTextStr = "";
String oneScreenTextStr = "";

void lcdPrint(char ch, Cursor* currentCursor, LiquidCrystal_I2C* lcd, byte lcdAddr) {
  byte* x = &(currentCursor->x);
  byte* y = &(currentCursor->y);
  ++(*x); // Update da posicao X do cursor
  if (*x > LINE_LENGTH) { // Se estiver no fim da linha
    ++(*y); // Pula linha
    *x = 1;
    if (*y == NUMBER_LINES) { // Se passou da ultima linha
      lcd->clear(); // Limpa tela
      if(lcdAddr == LCD_TEXT_ADDR) oneScreenTextStr = ""; // Reseta string
      else if(lcdAddr == LCD_MORSE_ADDR) oneScreenMorseStr = ""; // Reseta string
      // y zerado para cursor ir para 0,0
      *y = 0;
    }
    lcd->setCursor(0, *y);
  }
  lcd->print(ch); // Print
}

void dot() {
  lcdPrint('.', &cursorMorse, &lcdMorse, LCD_MORSE_ADDR);
  tone(PIN_BUZ, MORSE_FREQ, DURATION_DOT_SOUND);
}

void dash() {
  lcdPrint('-', &cursorMorse, &lcdMorse, LCD_MORSE_ADDR);
  tone(PIN_BUZ, MORSE_FREQ, DURATION_DASH_SOUND);
}

void morseCodeAtIndex(byte indexInDict) {
  byte* morseEntry = morseDictionary[indexInDict]; // Ponteiro para inicio do array de entrada no dicionario
  size_t sizeMorse = morseEntry[0]; // Tamanho do array indicado no primeiro index
  for (byte i = 1; i <= sizeMorse; ++i) {
    if(morseEntry[i]) { // Se for 1, barra(dash), se for 0, ponto(dot)
      dash();
      delay(DELAY_SOUND);
    } else {
      dot();
      delay(DELAY_SOUND);
    }
  }
  delay(DELAY_BETWEEN_LETTERS_SOUND); // Delay
  lcdPrint(' ', &cursorMorse, &lcdMorse, LCD_MORSE_ADDR);
}

void strToMorse(String str, Cursor* currentCursor, LiquidCrystal_I2C* lcd) {
  lcd->clear();
  byte* x = &(currentCursor->x);
  byte* y = &(currentCursor->y);
  *x = 0;
  *y = 0;
  lcd->setCursor(*x, *y);

  for (byte i = 0; i < str.length(); ++i) { // Percorre string
    char ch = tolower(str[i]); // Lowercase do char
    if (isdigit(ch)) morseCodeAtIndex(ch - OFFSET_DIGIT_ASCII); // Se for digito chama morseCodeAtIndex() com o index ajustado pelo offset para corresponder a entrada no dicionario
    else if (isalpha(ch)) morseCodeAtIndex(ch - OFFSET_LETTER_ASCII); // Se for letra chama morseCodeAtIndex() com o index ajustado pelo offset para corresponder a entrada no dicionario
  }
}

byte indexMatchedEntries() {
  size_t dictLength = sizeof(morseDictionary) / sizeof(morseDictionary[0]);
  for (byte i = 0; i < dictLength; ++i) {
    byte* code = morse.code;
    byte* entry = morseDictionary[i];
    byte morseLength = morse.i;
    byte entryLength = entry[0];
    bool isArraysEqual = 0;
    if (morseLength == entryLength) { // Se tamanhos forem iguais (+1 pois entrySize contem um a mais, o tamanho no primeiro index)
      isArraysEqual = 1;
      for (byte j = 0; j < entryLength; ++j) { // Percorre os dois arrays
        //Serial.println(code[j]);
        if (code[j] != entry[j + 1]) { // Pula o primeiro index de entry
          isArraysEqual = 0; // Arrays diferem
          break;
        }
      }
    }
    if (isArraysEqual) return i; // Se arrays forem iguais retorna index no dicionario
    else isArraysEqual = 1; // Se nao reset
  }
}

unsigned long startTime = 0;
bool isReleased = 1;
unsigned long timeOfRelease = 0;

unsigned long calculateTime() {
  if (digitalRead(PIN_BTN_MORSE_TO_TXT) == LOW) { // Botao pressionado
    
    if (startTime == 0) { // Se tempo inicial nao definido
      isReleased = 0;
      startTime = millis();
    }
  } else { // Botao solto
    if(!isReleased) {
      unsigned long time = millis() - startTime;
      startTime = 0; // Tempo inicial a ser definido novamente no futuro
      timeOfRelease = millis();
      isReleased = 1;
      return time;
    }
  }
    return 0;
}

unsigned long elapsedTime = 0;

byte signalType() {
    elapsedTime = calculateTime();
    bool isDash = (elapsedTime >= DELAY_DASH_INPUT);
    if (isDash) return DASH_STATE; // Se tiver passado o delay para ser barra
    bool isDot = (elapsedTime > DELAY_DEBOUNCE);
    if (isDot) return DOT_STATE; // Se tiver passado o delay para ser ponto
    bool isSkipToNextChar = ((millis() - timeOfRelease) >= DELAY_BETWEEN_INPUT);
    bool isMorseNotEmpty = ((morse.i) > 0);
    bool isMorseFull = ((morse.i) >= MORSE_LENGTH);
    // Se tiver passado o tempo para passar para outra letra, o botao nao esta pressionado
    // e morse nao esta vazio
    // ou morse estiver no fim
    if ((isSkipToNextChar && isReleased && isMorseNotEmpty) || isMorseFull) return PRINT_STATE;
    return IDLE_STATE; // Se nao retorna 0
}

char morseToChar() {
  char ch;
  byte i = indexMatchedEntries(); // Pega index da entrada equivalente no dicionario
  bool isDigit = (i > 25);
  if (isDigit) ch = i + OFFSET_DIGIT_ASCII; // Se for digito usa offset pra adquirir o valor equivalente na tabela ASCII
  else {
    ch = i + OFFSET_LETTER_ASCII; // Se for letra usa offset pra adquirir o valor equivalente na tabela ASCII
    ch = toupper(ch); // Transforma para letra maiuscula
  }
  morse.i = 0; // Reset do codigo morse pra uma letra
  entireTextStr += ch;
  oneScreenTextStr = entireTextStr;
  return ch; // Retorna caracter
}

void addMorse(byte val) {
  byte* code = morse.code;
  byte* i = &(morse.i);
  code[*i] = val;
  ++(*i);
  if (val) dash();
  else dot();
}

byte prevState = 0;
byte state = 0;

void morseToStrOnButtonPress() {
  if(state != IDLE_STATE) prevState = state;
  state = signalType();
  if (state == DOT_STATE) { // Se estiver na range para ser ponto
    addMorse(0);
    entireMorseStr += '.';
    oneScreenMorseStr = entireMorseStr;
  } else if (state == DASH_STATE) { // Se estiver na range para ser barra
    addMorse(1);
    entireMorseStr += '-';
    oneScreenMorseStr = entireMorseStr;
  
  } else if (state == PRINT_STATE) { // Se estiver na range para printar letra e passar para a proxima
    char ch = morseToChar();
    lcdPrint(' ', &cursorMorse, &lcdMorse, LCD_MORSE_ADDR);
    entireMorseStr += ' ';
    oneScreenMorseStr = entireMorseStr;
    lcdPrint(ch, &cursorText, &lcdText, LCD_TEXT_ADDR);
    lcdPrint(' ', &cursorText, &lcdText, LCD_TEXT_ADDR); // Espacamento entre letras
    entireTextStr += ' ';
    oneScreenTextStr = entireTextStr;
  } else return -1;
}

char changeCursor(char ch) {
  return ch == '|' ? ' ' : '|';
}

void blinkCursor(Cursor* currentCursor, LiquidCrystal_I2C* lcd) {
  byte* x = &(currentCursor->x);
  byte* y = &(currentCursor->y);
  char* cursorSymbol = &(currentCursor->cursorSymbol);
  unsigned long* lastCursorBlink = &(currentCursor->lastCursorBlink);
  if(state == 0 && *x < LINE_LENGTH) {
    if(millis() - *lastCursorBlink >= CURSOR_BLINK_DELAY) {
      *lastCursorBlink = millis();
      lcd->print(*cursorSymbol);
      lcd->setCursor(*x, *y);
      *cursorSymbol = changeCursor(*cursorSymbol);
    }
  }
}

void eraseMorse(Cursor* currentCursor, LiquidCrystal_I2C* lcd) {
  byte* x = &(currentCursor->x);
  byte* y = &(currentCursor->y);

  byte entireStrLen = entireMorseStr.length();
  int lastSpaceIndex = entireMorseStr.lastIndexOf(' ');
  int secondLastSpaceIndex = entireMorseStr.lastIndexOf(' ', lastSpaceIndex - 1);
   // Se for ultimo e secondLastSpaceIndex estiver fora da range apaga tudo
  if (secondLastSpaceIndex == -1) entireMorseStr = "";
  else entireMorseStr.remove(secondLastSpaceIndex + 1); // Corta string ate ultimo morse
  entireStrLen = entireMorseStr.length();
  unsigned int numCharsOnScreen = (entireStrLen % LCD_SIZE);
  if(entireStrLen > LCD_SIZE) {
    oneScreenMorseStr = entireMorseStr.substring(entireStrLen - numCharsOnScreen);
  } else {
    oneScreenMorseStr = entireMorseStr;
  }
  lcd->clear(); // Limpa tela
  *x = 0;
  *y = 0;
  size_t strLen = oneScreenMorseStr.length();
  for(byte i = 0; i<strLen; ++i) {
    lcdPrint(oneScreenMorseStr[i], currentCursor, lcd, LCD_MORSE_ADDR);
  }
}

void eraseLetter(Cursor* currentCursor, LiquidCrystal_I2C* lcd) {
  byte* x = &(currentCursor->x);
  byte* y = &(currentCursor->y);

  byte entireStrLen = entireTextStr.length();
  entireTextStr = entireTextStr.substring(0, entireStrLen - 2); // Corta string ate ultima letra
  entireStrLen = entireTextStr.length();
  unsigned int numCharsOnScreen = (entireStrLen % LCD_SIZE);
  oneScreenTextStr = entireTextStr.substring(entireStrLen - numCharsOnScreen, numCharsOnScreen + 1);
  lcd->clear(); // Limpa tela
  *x = 0;
  *y = 0;
  size_t strLen = oneScreenTextStr.length();
  for(byte i = 0; i<strLen; ++i) {
    lcdPrint(oneScreenTextStr[i], currentCursor, lcd, LCD_TEXT_ADDR);
  }
}

void erase() {
  if (digitalRead(PIN_BTN_ERASE) == LOW && prevState == PRINT_STATE) { // Se botao de apagar for apertado e estiver no estado de repouso
    if(entireTextStr.length() == 0) return;
    eraseLetter(&cursorText, &lcdText);
    eraseMorse(&cursorMorse, &lcdMorse);
    tone(PIN_BUZ, ERASE_FREQ, DURATION_DASH_SOUND);
    delay(DURATION_DASH_SOUND);
  }
}

void loop() {
  if (digitalRead(PIN_BTN_REPLAY) == LOW) { // Se botão de replay for apertado
    strToMorse(oneScreenTextStr, &cursorMorse, &lcdMorse);
    // strToMorse("The quick brown fox jumps over the lazy dog");
    // strToMorse("1234567890");
  }
  erase(); // Apaga se botão de apagar for apertado
  // Converte pressionamentos do botao em morse para string que sera exibida na tela LCD
  morseToStrOnButtonPress();
  blinkCursor(&cursorMorse, &lcdMorse); // Exibe cursor piscando no LCD do morse
  blinkCursor(&cursorText, &lcdText); // Exibe cursor piscando no LCD do texto
}
