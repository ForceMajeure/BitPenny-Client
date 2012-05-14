/*
	Bitpenny Client 0.2.5 BETA

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

#ifndef BITPENNY_STATS_H_
#define BITPENNY_STATS_H_
#include "boost/tuple/tuple.hpp"
#include "serialize.h"

using namespace std;
using namespace boost;

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define TUPLE3TYPE(t1, t2, t3)    tuple<t1, t2, t3>

string LocalDateTimeStrFormat(const char* pszFormat, int64 nTime);

class CServerStats
{
public:
    int nVersion;
    int64 nTime;
    int nConnectedClients;
    std::vector< pair<string,double> > vHashRate;
    int		nLastBlock; // last block
    int64   nLastBlockTime;
    int		nTotalBlocks;
    std::vector< boost::tuple<string,int64,int64> > vCounts;
    int64	nExcessFee;

    CServerStats()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nTime);
        READWRITE(nConnectedClients);
        READWRITE(vHashRate);
        READWRITE(nLastBlock);
        READWRITE(nLastBlockTime);
        READWRITE(nTotalBlocks);
        READWRITE(vCounts);
        READWRITE(nExcessFee);
    )

    void SetNull()
    {
        nVersion = 1;
        nTime = GetTime();
        nConnectedClients = 0;
        vHashRate.clear();
        nLastBlock = 0;
        nLastBlockTime = 0;
        nTotalBlocks = 0;
        vCounts.clear();
        nExcessFee = 0;
    }

    string HashRatesToStr()
    {
    	string strRes("(MH/s)");

    	BOOST_FOREACH(const PAIRTYPE(string, double)& item, vHashRate)
		{
    		strRes += strprintf(" %s=%.3f", item.first.c_str(), item.second);
		}

    	return strRes;
    }

    string CountsToStr()
    {
    	string strRes("shares(BTC)");

    	BOOST_FOREACH(const TUPLE3TYPE(string,int64,int64)& item, vCounts)
		{
    		strRes += strprintf(" %s=%lld(%s)", item.get<0>().c_str(), item.get<1>(), FormatMoney(item.get<2>()).c_str());
		}

    	return strRes;
    }

    string ToStr()
    {
    	string strRet = "statstime=" + LocalDateTimeStrFormat("%x %H:%M:%S", nTime) +
    					": hashrates=" + HashRatesToStr() +
    					": counts=" + CountsToStr() +
    					": credit=" + FormatMoney(nExcessFee) +
    					strprintf(": clients=%d: blocks=%d: lastblock=%d: ", nConnectedClients,  nTotalBlocks, nLastBlock)
    					+ LocalDateTimeStrFormat("%x %H:%M:%S", nLastBlockTime);
    	return strRet;
    }

};

class CClientStats
{
public:
    int nVersion;
    int64 nTime;
    std::vector< pair<string,double> > vHashRate;
    std::vector< boost::tuple<string,int64,int64> > vCounts;

    CClientStats()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nTime);
        READWRITE(vHashRate);
        READWRITE(vCounts);
    )

    void SetNull()
    {
        nVersion = 1;
        nTime = GetTime();
        vHashRate.clear();
        vCounts.clear();
    }
    string HashRatesToStr()
    {
    	string strRes("(MH/s)");

    	BOOST_FOREACH(const PAIRTYPE(string, double)& item, vHashRate)
		{
    		strRes += strprintf(" %s=%.3f", item.first.c_str(), item.second);
		}

    	return strRes;
    }

    string CountsToStr()
    {
    	string strRes("shares(BTC)");

    	BOOST_FOREACH(const TUPLE3TYPE(string,int64,int64)& item, vCounts)
		{
    		strRes += strprintf(" %s=%lld(%s)", item.get<0>().c_str(), item.get<1>(), FormatMoney(item.get<2>()).c_str());
		}

    	return strRes;
    }

    string ToStr()
    {
    	string strRet = "statstime=" + LocalDateTimeStrFormat("%x %H:%M:%S", nTime) +
    					": hashrates=" + HashRatesToStr() +
    					": counts=" +  CountsToStr();
    	return strRet;
    }
};

#endif /* BITPENNY_STATS_H_ */
