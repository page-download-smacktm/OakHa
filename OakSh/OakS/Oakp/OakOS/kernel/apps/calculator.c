#include "acorn/apps/calculator.h"
#include "acorn/framebuffer.h"

#define COLOR_WINDOW 0x1D303A
#define COLOR_TITLE 0x2A5B63
#define COLOR_TEXT 0xD8F3DC
#define COLOR_CURSOR 0xF4D35E

static void number_text(char *destination, unsigned long value, int negative)
{
    char reverse[24];
    unsigned int length = 0;
    unsigned int output = 0;
    if (value == 0) reverse[length++] = '0';
    while (value != 0) {
        reverse[length++] = (char)('0' + value % 10);
        value /= 10;
    }
    if (negative) destination[output++] = '-';
    while (length != 0) destination[output++] = reverse[--length];
    destination[output] = '\0';
}

int calculator_evaluate(const char *expression, char *output, unsigned int output_size)
{
    long result = 0;
    long value = 0;
    char operation = '+';
    unsigned int index = 0;
    int has_value = 0;
    if (output == (char *)0 || output_size < 2) return 0;
    while (1) {
        while (expression[index] == ' ') ++index;
        int negative = expression[index] == '-';
        if (negative) ++index;
        value = 0;
        has_value = 0;
        while (expression[index] >= '0' && expression[index] <= '9') {
            value = value * 10 + expression[index] - '0';
            ++index;
            has_value = 1;
        }
        if (!has_value) return 0;
        if (negative) value = -value;
        if (operation == '+') result += value;
        else if (operation == '-') result -= value;
        else if (operation == '*') result *= value;
        else if ((operation == '/' || operation == ':') && value != 0) result /= value;
        else return 0;
        while (expression[index] == ' ') ++index;
        if (expression[index] == '\0') break;
        operation = expression[index++];
        if (operation != '+' && operation != '-' && operation != '*' &&
            operation != '/' && operation != ':') return 0;
    }
    number_text(output, result < 0 ? (unsigned long)-result : (unsigned long)result,
        result < 0);
    return 1;
}

void calculator_draw(const char *output)
{
    framebuffer_fill_rect(318, 180, 410, 300, COLOR_WINDOW);
    framebuffer_fill_rect(318, 180, 410, 30, COLOR_TITLE);
    framebuffer_draw_text(334, 190, "CALCULATOR", COLOR_TEXT, 2);
    framebuffer_draw_text(336, 236, "TYPE: calc 12+30", COLOR_TEXT, 1);
    framebuffer_draw_text(336, 268, "RESULT", COLOR_CURSOR, 1);
    framebuffer_draw_text(336, 286, output, COLOR_TEXT, 2);
}
