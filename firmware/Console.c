//
//  Console for USB host
//
//
#include "Console.h"

#include "USBTerminal.h"
#include "SystemTime.h"
#include "CommandProcessor.h"
#include "StringUtils.h"
#include <avr/io.h>
#include <avr/pgmspace.h>

#define SINGLE_SCREEN 0

#define ANSI_ESCAPE_SEQUENCE(EscapeSeq)  "\33[" EscapeSeq
#define ESC_CURSOR_POS(Line, Column)    ANSI_ESCAPE_SEQUENCE(#Line ";" #Column "H")
#define ESC_ERASE_LINE                  ANSI_ESCAPE_SEQUENCE("K")
#define ESC_CURSOR_POS_RESTORE          ANSI_ESCAPE_SEQUENCE("u")

static const prog_char crP[] = {13,0};
static const prog_char crlfP[] = {13,10,0};

// state variables
CharString_define(40, commandBuffer)
static SystemTime_t nextStatusPrintTime;
static uint8_t currentPrintLine = 5;

static bool consoleIsConnected (void)
{
    return USBTerminal_isConnected();
}

void Console_Initialize (void)
{
    SystemTime_futureTime(0, &nextStatusPrintTime); // start right away
}

void Console_task (void)
{
    ByteQueue_t* rxQueue = &FromUSB_Buffer;
    if (!ByteQueue_is_empty(rxQueue)) {
        char cmdByte = ByteQueue_pop(rxQueue);
        switch (cmdByte) {
            case '\r' : {
                // command complete. execute it
                USBTerminal_sendCharsToHostP(crlfP);
                CharStringSpan_t command;
                CharStringSpan_init(&CommandProcessor_incomingCommand, &command);
                CommandProcessor_executeCommand(&command, &CommandProcessor_commandReply);
                if (!CharString_isEmpty(&CommandProcessor_commandReply)) {
                    Console_printCS(&CommandProcessor_commandReply);
                    CharString_clear(&CommandProcessor_commandReply);
                }
                CharString_clear(&CommandProcessor_incomingCommand);
                }
                break;
            case 0x7f : {
                CharString_truncate(CharString_length(&CommandProcessor_incomingCommand) - 1,
                    &CommandProcessor_incomingCommand);
                }
                break;
            default : {
                // command not complete yet. append to command buffer
                CharString_appendC(cmdByte, &CommandProcessor_incomingCommand);
                }
                break;
        }
        // echo current command
        USBTerminal_sendCharsToHostP(crP);
        USBTerminal_sendCharsToHostCS(&CommandProcessor_incomingCommand);
        USBTerminal_sendCharsToHost(ESC_ERASE_LINE);
    }

    // display status
    if (consoleIsConnected() &&
        SystemTime_timeHasArrived(&nextStatusPrintTime)) {
        CharString_define(80, statusMsg)
        CommandProcessor_createStatusMessage(&statusMsg);
        USBTerminal_sendCharsToHostP(crP);
        USBTerminal_sendCharsToHostCS(&statusMsg);
        USBTerminal_sendCharsToHostP(crlfP);

        if (!CharString_isEmpty(&CommandProcessor_incomingCommand)) {
            // echo current command
            USBTerminal_sendCharsToHostCS(&CommandProcessor_incomingCommand);
            USBTerminal_sendCharsToHost(ESC_ERASE_LINE);
        }

	// schedule next display
	SystemTime_futureTime(100, &nextStatusPrintTime);
    }
}

static void sendCursorTo (
    const int line,
    const int column)
{
    CharString_define(16, cursorToBuf);
    CharString_copyP(PSTR("\33["), &cursorToBuf);
    StringUtils_appendDecimal(line, 1, 0, &cursorToBuf);
    CharString_appendC(';', &cursorToBuf);
    StringUtils_appendDecimal(column, 1, 0, &cursorToBuf);
    CharString_appendC('H', &cursorToBuf);
    USBTerminal_sendCharsToHostCS(&cursorToBuf);
}

void Console_print (
    const char* text)
{
    if (USBTerminal_isConnected ()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
            currentPrintLine = 5;
            USBTerminal_sendCharsToHost(ESC_CURSOR_POS(5, 1));
            USBTerminal_sendCharsToHost(ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif
        // print text on the current line
	sendCursorTo(currentPrintLine, 1);
#endif
        USBTerminal_sendLineToHost(text);
#if SINGLE_SCREEN
        USBTerminal_sendCharsToHost(ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}

void Console_printP (
    PGM_P text)
{
    if (USBTerminal_isConnected ()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
            currentPrintLine = 5;
            USBTerminal_sendCharsToHost(ESC_CURSOR_POS(5, 1));
            USBTerminal_sendCharsToHost(ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif

	// print text on the current line
	sendCursorTo(currentPrintLine, 1);
#endif
        USBTerminal_sendLineToHostP(text);
#if SINGLE_SCREEN
        USBTerminal_sendCharsToHost(ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}

void Console_printCS (
    const CharString_t *text)
{
    CharStringSpan_t textSpan;
    CharStringSpan_init(text, &textSpan);
    Console_printCSS(&textSpan);
}

void Console_printCSS (
    const CharStringSpan_t *text)
{
    if (consoleIsConnected()) {
#if SINGLE_SCREEN
#if 0
        if (currentPrintLine > 22) {
	        currentPrintLine = 5;
	        USBTerminal_sendCharsToHost(ESC_CURSOR_POS(5, 1));
	        USBTerminal_sendCharsToHost(ANSI_ESCAPE_SEQUENCE("J"));
        }
#endif
        // print text on the current line
        sendCursorTo(currentPrintLine, 1);
#endif
        USBTerminal_sendLineToHostCSS(text);
#if SINGLE_SCREEN
        USBTerminal_sendCharsToHost(ESC_CURSOR_POS_RESTORE);
        ++currentPrintLine;

        // restore cursor to command buffer end
        sendCursorTo(1, CharString_length(&commandBuffer)+1);
#endif
    }
}
