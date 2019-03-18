//this file is part of MimeTools (plugin for Notepad++)
//Copyright (C)2019 Don HO <don.h@free.fr>
//
//
// Enhance Base64 features, and rewrite Base64 encode/decode implementation
// Copyright 2019 by Paul Nankervis <paulnank@hotmail.com>
//
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginInterface.h"
#include "mimeTools.h"
#include "b64.h"
#include "qp.h"
#include "url.h"
#include "saml.h"

// Base64 encoding decoding - where 8 bit ascii is re-represented using just 64 ascii characters (plus optional padding '=').
//
// This code includes options to encode to base64 in multiple ways. For example the text lines:-
//
//  If you can keep your head when all about you
//  Are losing theirs and blaming it on you;
//
// Using "Encode with Unix EOL" would produce a single base64 string with line breaks after each 64 characters:-
//
//  SWYgeW91IGNhbiBrZWVwIHlvdXIgaGVhZCB3aGVuIGFsbCBhYm91dCB5b3UNCkFy
//  ZSBsb3NpbmcgdGhlaXJzIGFuZCBibGFtaW5nIGl0IG9uIHlvdTs=
//
// That would be decoded using a single base64 decode which ignored whitespace characters (the line breaks).
//
// Alternatively the same lines could be encoded using a "by line" option to encode each line of input as
// its own separate base64 string:-
//
//  SWYgeW91IGNhbiBrZWVwIHlvdXIgaGVhZCB3aGVuIGFsbCBhYm91dCB5b3U
//  QXJlIGxvc2luZyB0aGVpcnMgYW5kIGJsYW1pbmcgaXQgb24geW91Ow
//
// Each of these output lines could be decoded separately, or multiple lines decoded using byLine
// (technically "noWhiteSpace") to cause base64 decoding to restart on each line (or after each whitespace)
//
// Test cases:-
// Strict mode decoding SUCCESS use cases (allowing whitespace):-
// QUJD
// QUJDZA==
// QUJDZGU=
// QUJDZGVm
//  QU JD
//     Q U J D Z A     = =
// QU JDZ  GU            =
//  Q  U   J   D   Z   G   V   m
//
// Strict mode decoding FAIL use cases:-
// QUJD=
// QUJDZA=
// QUJDZGU = =
// QUJDZGVm  =
// QUJD   =     QUJD
// QUJDZA===
// QUJDZGU
// QUJDZGVm == =


char base64CharSet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int base64CharMap[] = {  // base64 values or: -1 for illegal character, -2 to ignore character, and -3 for pad ('=')
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -1, -1, -2, -1, -1,  // <tab> <lf> & <cr> are ignored
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  // <space> is ignored
    52, 53, 54, 55 ,56, 57, 58, 59, 60, 61, -1, -1, -1, -3, -1, -1,  // '=' is the pad character
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1 ,-1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

// base64Encode simply converts ascii to base64 with appropriate wrapping and padding. Encoding is done by loading
// three ascii characters at a time into a bitField, and then extracting them as four base64 values.
// returnString is assumed to be large enough to contain the result (which is typically 4 / 3 the input size
// plus line breaks), and the function return is the length of the result.
// wrapLength sets the length at which to wrap the encoded test at (not valid with byLineFlag)
// padFlag controls whether the one or two '=' pad characters are included at the end of encoded strings
// byLineFlag causes each input line to be encoded as a separate base64 string
// Note: byLine will eat output buffer at a greater rate than regular encoding. In the extreme case one character
// per line will convert each 2 input characters (character + line terminator) to 3 output characters, giving a total of
// 3 / 2 the input size. With padding turned on that would become 5 characters, giving an output 5 / 2 of the input size.
// This needs to be allowed for in the size of the output buffer!!!


int base64Encode(char *resultString, const char *asciiString, size_t asciiStringLength, size_t wrapLength, bool padFlag, bool byLineFlag)
{
    size_t index; // input string index
    size_t lineLength = 0; // current output line length
    int resultLength = 0, // result string length
        bitField, // assembled bit field (up to 3 ascii characters at a time)
        bitOffset, // offset into bit field (8 bit input: 16, 8, 0 -> 6 bit output: 18, 12, 6, 0)
        endOffset, // end offset index value
        charValue = 0; // character value

    for (index = 0; index < asciiStringLength; )
    {
        bitField = 0;
        for (bitOffset = 16; bitOffset >= 0 && index < asciiStringLength; bitOffset -= 8)
        {
            charValue = (UCHAR)asciiString[index++];
            if (byLineFlag && (charValue == '\n' || charValue == '\r'))
            {
                break; // deal with EOL later
            }
            bitField |= charValue << bitOffset; // load character into bit field
        }
        if (bitOffset < 16) // Anything in bit field?
        {
            endOffset = bitOffset + 3; // end indicator
            for (bitOffset = 18; bitOffset > endOffset; bitOffset -= 6)
            {
                if (!byLineFlag && wrapLength > 0 && lineLength++ >= wrapLength)
                {
                    resultString[resultLength++] = '\n';
                    lineLength = 1;
                }
                resultString[resultLength++] = base64CharSet[(bitField >> bitOffset) & 0x3f];
            }
            if (padFlag) // write padding if required
            {
                for (; bitOffset >= 0; bitOffset -= 6)
                {
                    if (!byLineFlag && wrapLength > 0 && lineLength++ >= wrapLength)
                    {
                        resultString[resultLength++] = '\n';
                        lineLength = 1;
                    }
                    resultString[resultLength++] = '=';
                }
            }
        }
        if (byLineFlag && (charValue == '\n' || charValue == '\r'))
        {
            resultString[resultLength++] = (char)charValue; // write EOL
        }
    }
    return resultLength;
}

// base64Decode converts base64 to ascii. But there are choices about what to do with illegal characters or
// malformed strings. In this version there is a strict flag to indicate that the input must be a single
// valid base64 string with no illegal characters, correct padding, and no short segments. Otherwise
// there is best effort to decode around illegal characters which ARE preserved in the output.
// So  "TWFyeQ=aGFk=YQ=bGl0dGxl=bGFtYg=" decodes to "Maryhadalittlelamb" because each base64 segment
// is terminated by the pad character "=". "TWFyeQ==.aGFk.YQ.bGl0dGxl.bGFtYg=="  would decode to
// "Mary.had.a.little.lamb" because each of the five base64 strings is separated by the illegal
// character dot. In strict mode the first dot would trigger a fatal error. Some other implementations
// choose to ignore illegal characters which of course has it's own issues.
// The four whitespace characters <CR> <LF> <TAB> and <SPACE> are silently ignored unless noWhitespace
// is set. In this case whitespace is treated similar to illegal characters and base64 decoding operates
// around the white space. So "TWFyeQ== aGFk YQ bGl0dGxl bGFtYg==" would decode as "Mary had a little lamb".
// This also gives a 'byLine' capability to decode each line by writing the EOL and restarting decoding.
// Decoding is done by loading four base64 characters at a time into a bitField, and then extracting them as
// three ascii characters.
// returnString is assumed to be large enough to contain the result (which could be the same size as the input),
// and the function return is the length of the result, or a negative value in case of an error.

int base64Decode(char *resultString, const char *encodedString, size_t encodedStringLength, bool strictFlag, bool noWhiteSpace)
{
    size_t index; // input string index

    int resultLength = 0, // result string length
        bitField, // assembled bit field (up to 3 ascii characters at a time)
        bitOffset, // offset into bit field (6 bit intput: 18, 12, 6, 0 -> 8 bit output: 16, 8, 0)
        endOffset, // end offset index value
        charValue = 0, // character value
        charIndex = 0; // character index

    for (index = 0; index < encodedStringLength; )
    {
        bitField = 0;
        for (bitOffset = 18; bitOffset >= 0 && index < encodedStringLength; )
        {
            charValue = (UCHAR)encodedString[index++];
            charIndex = base64CharMap[charValue & 0x7f];
            if (charIndex >= 0)
            {
                bitField |= charIndex << bitOffset; // put data in bit field
                bitOffset -= 6;
            }
            else
            {
                if (charIndex != -2 || noWhiteSpace)
                {
                    break;// exit loop if not whitespace
                }
            }
        }
        if (strictFlag && bitOffset >= 0)  // pedantic mode checks when bit field didn't fill
        {
            if (charIndex == -1 || (charIndex == -2 && noWhiteSpace)) // Bad input data
            {
                return -1; // **ERROR** Bad character in input string
            }
            if (charIndex != -3) // If  not pad (must have run out of input)
            {
                    if (bitOffset != 18) // Check if any data in bit field
                    {
                        return -2; // **ERROR** Not enough data
                    }
            }
            else  // got pad so check what comes after
            {
                endOffset = bitOffset;
                while (index < encodedStringLength)
                {
                    charValue = (UCHAR)encodedString[index++];
                    charIndex = base64CharMap[charValue & 0x7f];
                    if (charIndex == -3)
                    {
                        endOffset -= 6; // remember each pad - should complete with endOffset == 0
                    }
                    else
                    {
                        if (charIndex != -2 || noWhiteSpace)
                        {
                            return -3; // **ERROR** Data after pad character
                        }
                    }
                }
                if (bitOffset > 6 || endOffset != 0) // check for insufficient data or wrong padding
                {
                    return -4; // **ERROR** Incorrect padding
                }
            }
            charIndex = 0; // Strict checking completed - don't copy character at exit
        }

        endOffset = bitOffset + 3; // end indicator
        for (bitOffset = 16; bitOffset > endOffset; bitOffset -= 8)
        {
            resultString[resultLength++] = (bitField >> bitOffset) & 0xff; // get character from bit field
        }

        if (charIndex == -1 || charIndex == -2) // Was there an illegal character or space?
        {
            resultString[resultLength++] = (char)charValue; // Copy illegal character to output string
        }
    }
    return resultLength;
}