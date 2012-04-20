/*
	Bitpenny Client

	Copyright (c) 2011 ForceMajeure & OneFixt (www.BitPenny.com)


	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/


#ifndef BITCOIN_BITPENNY_CLIENT_H
#define BITCOIN_BITPENNY_CLIENT_H

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include "bitpenny_stats.h"
#include "strlcpy.h"
#include "init.h"

using namespace std;
using namespace json_spirit;

extern bool fBitpennyPoolMode;
extern bool fMiningSolo;
extern bool fDisableSoloMode;

extern CNode* pnodeBitpennyHost; // from net.cpp

extern unsigned int nBitpennyPoolDifficulty; // default 8

extern uint256 hashBitpennyPoolTarget;
extern double dBitpennyPoolFeePerCent;
extern string strBitpennyPoolName;

extern unsigned int nBitpennyPoolExtraNonceBase;

bool BitpennyInit();

void CheckPoolConnection();

Value getpool(const Array& params, bool fHelp);
Value setpool(const Array& params, bool fHelp);
Value getdisablesolo(const Array& params, bool fHelp);
Value setdisablesolo(const Array& params, bool fHelp);
void  bitpennyinfo(Object& obj);
Value getwork(const Array& params, bool fHelp);
Value setstatsinterval(const Array& params, bool fHelp);
Value setprintblocks(const Array& params, bool fHelp);

bool ProcessBitpennyMessage(CNode* pfrom, string strCommand, CDataStream& vRecv);

// block monitor
extern bool fBlockMonitor;
void BlockMonitor(const char* msg=NULL);
extern string strRPCPeerAddress;
Value getblockmonitor(const Array& params, bool fHelp);
Value setblockmonitor(const Array& params, bool fHelp);
Value setblockmonitortarget(const Array& params, bool fHelp);
Value delblockmonitortarget(const Array& params, bool fHelp);
Value listblockmonitortargets(const Array& params, bool fHelp);

#endif
