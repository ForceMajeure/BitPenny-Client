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

#include "net.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#include <map>
#include <set>
#include "bitpenny_client.h"
#include <boost/lexical_cast.hpp>

using namespace std;

string FormatVersion(int nVersion);

int nBitpennyClientVersion = 50201;

// bitpenny connection details
bool fBitpennyPoolMode = false;
CNode* pnodeBitpennyHost = NULL;

bool fDisableSoloMode = false;

string strBitpennyPoolHost;
int64 nBitpennyPoolPort = 0;

string  strPoolUserId;
CScript scriptMyPubKey;

unsigned int nBitpennyPoolDifficulty = 0;
uint256 hashBitpennyPoolTarget;
double dBitpennyPoolFeePerCent;
string strBitpennyPoolName;
unsigned int nBitpennyPoolExtraNonceBase = 0;

int nCurrentBlockCandidate = 0;
bool fNewPoolBlockIsAvailable = false;


// Block Monitor
bool fBlockMonitor = false;
string strRPCPeerAddress;
set<CService> sBlockMonitorTargets;
CCriticalSection cs_sBlockMonitorTargets;

struct BlockCandidate
{
	CBlock  		block;
	unsigned int    nBlockId;
	int				nBlockNumber;
};

list<BlockCandidate> lBlockCandidates;
set<unsigned int> setBlockCandidates;
CCriticalSection cs_lBlockCandidates;

CServerStats ServerStats;
CClientStats ClientStats;
CCriticalSection cs_ServerStats;

// stats
static int64	nStartTime;

static int64	nStatsInterval=60000;
static int64	nHashTimerStart = 0;
static unsigned int nHashCounter = 0;
static double   dPoolClientHashRate = 0.0;

static unsigned int nHashesReceived = 0;
static unsigned int nSubmittedForVerification=0;
static unsigned int nHashesVerified = 0;
static unsigned int nHashesRejected = 0;
static unsigned int nGetWorkCount = 0;
static int64 nSessionCredit = 0;
static int64 nMyShareInCurrentBlock = 0;
static int64 nMinerBounty = 0;

static int64 nTimeStartedMiningSolo;
static int64 nTimeMiningSolo = 0;

bool fMiningSolo = false;

static string strBitpennyPoolMessage;

static bool fPrintBlocks = false; // print block candidates to a log

inline bool IsConnectedToBitpenny()
{
	//CRITICAL_BLOCK(cs_vNodes)
		return (pnodeBitpennyHost != NULL && pnodeBitpennyHost->hSocket != INVALID_SOCKET && pnodeBitpennyHost->hSocket != SOCKET_ERROR && pnodeBitpennyHost->fDisconnect == false && pnodeBitpennyHost->fSuccessfullyConnected == true);
}

///////////////////////////////////////
// instead of util.cpp
///////////////////////////////////////
#ifdef WIN32
string LocalDateTimeStrFormat(const char* pszFormat, int64 nTime)
{
    time_t n = nTime;
    struct tm *tmTime ;

    tmTime = localtime(&n);

    char pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, tmTime);
    return pszTime;
}

string ElapsedTime(int64 nTime)
{
    time_t n = nTime;
    struct tm *tmTime;
    tmTime = gmtime(&n);
    return tmTime->tm_yday>1?
    		strprintf("%d:%02d:%02d:%02d", tmTime->tm_yday-1, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec) :
    		strprintf("%02d:%02d:%02d", tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec);
}


void MinerLog(const char* pszFormat, ...)
{
	int ret = 0;
	static FILE* fileout = NULL;
	static int nDayOpened = 0;
	struct tm *tmTime ;

	time_t nTime = GetTime();

	tmTime = localtime(&nTime);

	if (!fileout || nDayOpened != tmTime->tm_yday)
	{
		if(fileout) fclose(fileout);

		char pszTime[200];
		strftime(pszTime, sizeof(pszTime), "miner-%Y%m%d.log", tmTime);
		boost::filesystem::path pathMiner = GetDataDir() / pszTime;
		fileout = fopen(pathMiner.string().c_str(), "a");
		if (fileout) setbuf(fileout, NULL); // unbuffered
		nDayOpened = tmTime->tm_yday;
	}

	if (fileout)
	{
		//// Debug print useful for profiling
		fprintf(fileout, "%"PRI64d": ", GetTime());
		va_list arg_ptr;
		va_start(arg_ptr, pszFormat);
		ret = vfprintf(fileout, pszFormat, arg_ptr);
		va_end(arg_ptr);
	}
}
#else
string LocalDateTimeStrFormat(const char* pszFormat, int64 nTime)
{
    time_t n = nTime;
    struct tm tmTime ;

    localtime_r(&n, &tmTime);

    char pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, &tmTime);
    return pszTime;
}

string ElapsedTime(int64 nTime)
{
    time_t n = nTime;
    struct tm tmTime;
    gmtime_r(&n, &tmTime);
    return tmTime.tm_yday>1?
    		strprintf("%d:%02d:%02d:%02d", tmTime.tm_yday-1, tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec) :
    		strprintf("%02d:%02d:%02d", tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec);
}


void MinerLog(const char* pszFormat, ...)
{
	int ret = 0;
	static FILE* fileout = NULL;
	static int nDayOpened = 0;
    struct tm tmTime ;

	time_t nTime = GetTime();

    localtime_r(&nTime, &tmTime);

    static boost::mutex mutexMinerLog;
    boost::mutex::scoped_lock scoped_lock(mutexMinerLog);

	if (!fileout || nDayOpened != tmTime.tm_yday)
	{
		if(fileout) fclose(fileout);

	    char pszTime[200];
	    strftime(pszTime, sizeof(pszTime), "miner-%Y%m%d.log", &tmTime);
		boost::filesystem::path pathMiner = GetDataDir() / pszTime;
		fileout = fopen(pathMiner.string().c_str(), "a");
		if (fileout) setbuf(fileout, NULL); // unbuffered
		nDayOpened = tmTime.tm_yday;
	}

	if (fileout)
	{
		//// Debug print useful for profiling
		fprintf(fileout, "%"PRI64d": ", GetTime());
		va_list arg_ptr;
		va_start(arg_ptr, pszFormat);
		ret = vfprintf(fileout, pszFormat, arg_ptr);
		va_end(arg_ptr);
	}
}
#endif

////////////////////////////////////////
// for init.cpp
////////////////////////////////////////

static void LoadBlockMonitorTargets();

bool BitpennyInit()
{

    // Bitpenny start
	strPoolUserId = mapArgs["-pooluserid"];
	CBitcoinAddress address(strPoolUserId);

    if (strPoolUserId == "" || !address.IsValid())
    {
    	fprintf(stderr, "pooluserid=%s\n", strPoolUserId.c_str());
        fprintf(stderr, "Warning: make sure pooluserid is set to a valid Bitcoin Address in the configuration file: %s\n", GetConfigFile().string().c_str());
        return false;
    }

    scriptMyPubKey.SetBitcoinAddress(address);

    if (mapArgs["-poolhost"] == "")
    {
    	fprintf(stderr, "Warning: make sure poolhost is set in the configuration file: %s\n", GetConfigFile().string().c_str());
    	return false;
    }

    strBitpennyPoolHost = mapArgs["-poolhost"];

    nBitpennyPoolPort = (int64)GetArg("-poolport", (fTestNet? 18338 : 8338));

    fBitpennyPoolMode = GetBoolArg("-pool");

    printf("fPoolMode = %d\n", fBitpennyPoolMode);
    printf("poolhost = %s\n", strBitpennyPoolHost.c_str());
    printf("poolport = %lld\n", nBitpennyPoolPort);
    printf("pooluserid = %s\n", strPoolUserId.c_str());

    nStartTime = GetTime();
    nStatsInterval = GetArg("-statsinterval", 60000); // 1 minute

    fDisableSoloMode = GetBoolArg("-pooldisablesolo");

    fBlockMonitor = GetBoolArg("-blockmonitor");
    if (fBlockMonitor)
    	LoadBlockMonitorTargets();

    return true;
}


////////////////////////////////////////
// net.cpp
////////////////////////////////////////

bool OpenNetworkConnection(const CAddress& addrConnect, bool fUseGrant = true);

bool ConnectToPool()
{
	if (pnodeBitpennyHost != NULL)
	{
		if (pnodeBitpennyHost->hSocket != INVALID_SOCKET && pnodeBitpennyHost->fDisconnect == false)
		{
			MinerLog("Already connected\n");
			return true;
		}
		pnodeBitpennyHost->Release();    // allow to delete from Disconnected Nodes LIst
		pnodeBitpennyHost = NULL;
	}

	CAddress addr(CService(strBitpennyPoolHost, nBitpennyPoolPort, true));
	if (addr.IsValid())
	{
		MinerLog("Connecting to %s\n", addr.ToStringIPPort().c_str());
		OpenNetworkConnection(addr, false); // BitPenny server is in addition to normal outbound connections
		Sleep(500);

		pnodeBitpennyHost = FindNode(addr);

		if (pnodeBitpennyHost != NULL)
		{
			MinerLog("login: %s\n", strPoolUserId.c_str());
			pnodeBitpennyHost->PushMessage("bp:login", strPoolUserId, nBitpennyClientVersion);
			pnodeBitpennyHost->AddRef();  // one more ref to keep it from disappearing
			return true;
		}
		MinerLog("Connection failed\n");
	}
	else
	{
		MinerLog("Could not initiate connection to a pool. Please check poolhost and poolport in config file\n");
		// fBitpennyPoolMode = false;
	}

    return false;
}

void DisconnectFromPool()
{
	if (pnodeBitpennyHost != NULL && pnodeBitpennyHost->fSuccessfullyConnected)
	{
		MinerLog("Disconnecting from pool\n");
		pnodeBitpennyHost->fSuccessfullyConnected = false;
		pnodeBitpennyHost->fDisconnect = true;  //deferred shutdown
		pnodeBitpennyHost->Release();    		// allow to delete
		pnodeBitpennyHost = NULL;
	}
}

static int64 nTimeLastConnectionAttemp = 0;

void CheckPoolConnection()
{
    static bool fNotified = false;
	int64 nCurrentTime = GetTime();

	if (pnodeBitpennyHost == NULL || pnodeBitpennyHost->hSocket == INVALID_SOCKET)
	{
		// connection to pool was lost while still working on pool block
		if (fBlockMonitor && !fMiningSolo && !fNotified)
		{
				BlockMonitor("Stale Block");
				fNotified = true;
		}

		// limit automatic re-connect attempts to once a minute.
		if (nCurrentTime - nTimeLastConnectionAttemp < 60)
			return;

		if (ConnectToPool())
			fNotified = false;

		nTimeLastConnectionAttemp = nCurrentTime;
	}
}

////////////////////////////////////////
// rpc.cpp
///////////////////////////////////////

static void UsePool(bool fPool)
{
	fBitpennyPoolMode = fPool;

	if(fPool)
		ConnectToPool();
	else
	{
		DisconnectFromPool();

		// disconnecting from pool but still working on pool block
		if (fBlockMonitor && !fMiningSolo)
			BlockMonitor("Stale Block");
	}
}


Value getpool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpool\n"
            "Returns true or false.");

    return (bool)fBitpennyPoolMode;
}


Value setpool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setpool <use pool>\n"
            "<use pool> is true or false to turn pooled mining on or off.\n");

    bool fPool = params[0].get_bool();
    if (fPool != fBitpennyPoolMode)
    	UsePool(fPool);
    return Value::null;
}

Value getdisablesolo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpooldisablesolo\n"
            "Returns true or false.");

    return (bool)fDisableSoloMode;
}


Value setdisablesolo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setpooldisablesolo <disable solo>\n"
            "<disable solo> is true or false.\n");

    fDisableSoloMode = params[0].get_bool();
    return Value::null;
}

Value setprintblocks(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setprintblocks <trace block candidates>\n"
            "<trace block candidates> is true or false to turn on or off printing block candidates information to a debug log.\n");

    fPrintBlocks = params[0].get_bool();
    return Value::null;
}

Value setstatsinterval(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("setstatsinterval <milliseconds>\n");

    nStatsInterval = params[0].get_int64();

    return Value::null;
}

Value getblockmonitor(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockmonitor\n"
            "Returns true or false.");

    return (bool)fBlockMonitor;
}

Value setblockmonitor(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "setblockmonitor <monitor>\n"
            "<monitor> is true or false to turn block monitor notificatrions on or off.");

    bool fMonitor = params[0].get_bool();

    if (fBlockMonitor != fMonitor)
    {
    	if (fMonitor)
    		LoadBlockMonitorTargets();  //re-load static targets
    	else
    	{
    		LOCK(cs_sBlockMonitorTargets);
    		sBlockMonitorTargets.clear();
    	}
    	fBlockMonitor = fMonitor;
    }

    return Value::null;
}

Value setblockmonitortarget(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("setblockmonitortarget [<host>] <udp port>\n");

    string strHost;
    int64 nPort;

    if (params.size() == 1)
    {
    	strHost = strRPCPeerAddress;
    	nPort = params[0].get_int64();
    }
    else
    {
    	strHost = params[0].get_str();
    	nPort = params[1].get_int64();
    }

    CService target(strHost, nPort, fAllowDNS);
    {
    	LOCK(cs_sBlockMonitorTargets);
    	sBlockMonitorTargets.insert(target);
    }
    return Value::null;
}

Value delblockmonitortarget(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("delblockmonitortarget [<host>] <udp port>\n");

    string strHost;
    int64 nPort;

    if (params.size() == 1)
    {
    	strHost = strRPCPeerAddress;
    	nPort = params[0].get_int64();
    }
    else
    {
    	strHost = params[0].get_str();
    	nPort = params[1].get_int64();
    }

    CService target(strHost, nPort, fAllowDNS);
    {
    	LOCK(cs_sBlockMonitorTargets);
    	sBlockMonitorTargets.erase(target);
    }
    return Value::null;
}


Value listblockmonitortargets(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw runtime_error("listblockmonitortargets\n");

    Array ret;
    {
    	LOCK(cs_sBlockMonitorTargets);
		BOOST_FOREACH(CService target, sBlockMonitorTargets)
		{
    		Object obj;
            obj.push_back(Pair("target", target.ToStringIPPort()));
            ret.push_back(obj);
		}
    }
    return ret;
}


Value ValueFromAmount(int64 amount);

void bitpennyinfo(Object& obj)
{
	int64 ntimesolo = (fMiningSolo && nTimeStartedMiningSolo)? GetTimeMillis() - nTimeStartedMiningSolo : 0;

	obj.push_back(Pair("pool",      	  			(bool)fBitpennyPoolMode));
	obj.push_back(Pair("poolclient",	  			strprintf("Bitpenny client %s", FormatVersion(nBitpennyClientVersion).c_str())));
	obj.push_back(Pair("poolclientuptime", 			ElapsedTime(GetTime() - nStartTime)));

	obj.push_back(Pair("pooldisablesolo", 	  		(bool)fDisableSoloMode));
	obj.push_back(Pair("miningsolo", 	  			(bool)fMiningSolo));
	obj.push_back(Pair("miningsolotime", 	  		ElapsedTime(ntimesolo / 1000 )));
	obj.push_back(Pair("miningsolotimesession", 	ElapsedTime((nTimeMiningSolo + ntimesolo) /1000)));

	obj.push_back(Pair("poolconnection",  			IsConnectedToBitpenny()? "up" : "down"));
	obj.push_back(Pair("poolname",        			strBitpennyPoolName));
	obj.push_back(Pair("poolfee(%)",     			(double)dBitpennyPoolFeePerCent));
	obj.push_back(Pair("pooldifficulty", 			(int)nBitpennyPoolDifficulty));
	obj.push_back(Pair("pooltarget",     			hashBitpennyPoolTarget.GetHex()));
	obj.push_back(Pair("poolsharevalue", 			ValueFromAmount(nMinerBounty)));

	{
		LOCK(cs_ServerStats);
		obj.push_back(Pair("poolstatstime",			LocalDateTimeStrFormat("%x %H:%M:%S", ServerStats.nTime)));
		obj.push_back(Pair("poolhashrates",			ServerStats.HashRatesToStr()));
		obj.push_back(Pair("poolcounts",			ServerStats.CountsToStr()));
		obj.push_back(Pair("poolcredit",			FormatMoney(ServerStats.nExcessFee)));
		obj.push_back(Pair("poolconnectedclients", 	(int)ServerStats.nConnectedClients));
		obj.push_back(Pair("pooltotalblocks", 		(int)ServerStats.nTotalBlocks));
		obj.push_back(Pair("poollastblock", 		(int)ServerStats.nLastBlock));
		obj.push_back(Pair("poollastblocktime",		LocalDateTimeStrFormat("%x %H:%M:%S", ServerStats.nLastBlockTime)));

		obj.push_back(Pair("clientstatstime",		LocalDateTimeStrFormat("%x %H:%M:%S", ClientStats.nTime)));
		obj.push_back(Pair("clienthashrates",		ClientStats.HashRatesToStr()));
		obj.push_back(Pair("clientcounts",			ClientStats.CountsToStr()));
	}

	obj.push_back(Pair("myaddress",       			strPoolUserId));

	// session stats nGetWorkCount
	obj.push_back(Pair("sessiongetworkcount",     	(int)nGetWorkCount));
	obj.push_back(Pair("sessionsharesfound",     	(int)nHashesReceived));
	obj.push_back(Pair("sessionsharespersec",     	(double)dPoolClientHashRate));
	obj.push_back(Pair("sessionstatsinterval", 		(boost::int64_t)nStatsInterval));

	obj.push_back(Pair("sessionsharessubmitted", 	(int)nSubmittedForVerification));
	obj.push_back(Pair("sessionsharesverified", 	(int)nHashesVerified));
	obj.push_back(Pair("sessionsharesrejected", 	(int)nHashesRejected));

	double dStaleShares = nHashesReceived>0? 100.0 * nHashesRejected / nHashesReceived : 0.0;

	obj.push_back(Pair("sessionstaleshares(%)", 	(double)dStaleShares));
	obj.push_back(Pair("sessioncredit",  			ValueFromAmount(nSessionCredit)));

	obj.push_back(Pair("projectedblockcredit",     	ValueFromAmount(nMyShareInCurrentBlock)));

	obj.push_back(Pair("poolmessage",        		strBitpennyPoolMessage));

	obj.push_back(Pair("printblockcandidates",		(bool)fPrintBlocks));
	obj.push_back(Pair("blockmonitor",				(bool)fBlockMonitor));

	obj.push_back(Pair("myexternalip",				addrLocalHost.ToStringIP()));
	obj.push_back(Pair("myrpcport",					(int)GetArg("-rpcport", 8332)));

}


Object JSONRPCError(int code, const string& message); // defined in rpc.cpp
bool ProcessBlock(CNode* pfrom, CBlock* pblock);      // defined in main.cpp


bool MinerCheckWork(uint256& hash, uint256& hashTarget, CBlock* pblock, CWallet& wallet)
{
    // if (fDebug)	printf("MinerCheckWork: \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());

    if (hash > hashTarget)
        return false;

    //// debug print
    printf("BitcoinMiner:\n");
    printf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
    pblock->print();
    printf("%s ", DateTimeStrFormat("%x %H:%M", GetTime()).c_str());
    printf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue).c_str());

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != hashBestChain)
            return error("BitcoinMiner : generated block is stale");

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[pblock->GetHash()] = 0;
        }

        // Process this block the same as if we had received it from another node
        if (!ProcessBlock(NULL, pblock))
            return error("BitcoinMiner : ProcessBlock, block not accepted");
    }

    return true;
}

struct WorkItem
{
	CBlock* pblock;
	unsigned int   nBlockId;
	bool    fPool;

	WorkItem()
	{
		pblock = NULL;
		nBlockId = 0;
		fPool = false;
	}
	WorkItem(CBlock* p)
	{
		pblock = p;
		nBlockId = p->nTime;
		fPool = false;
	}

	WorkItem(BlockCandidate* p)
	{
		pblock = new CBlock(p->block);
		nBlockId = p->nBlockId;
		fPool = true;
	}

	~WorkItem()
	{
		if (pblock && fPool)
			delete(pblock);
	}
};

static void MinerIncrementExtraNonce(CBlock* pblock, unsigned int& nExtraNonce, WorkItem *pwi)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }

    ++nExtraNonce;

    pblock->vtx[0].vin[0].scriptSig = CScript() << pwi->nBlockId << CBigNum(nExtraNonce);  // DO NOT CHANGE!
    if (pwi->fPool)
    	pblock->vtx[0].vin[0].scriptSig << CBigNum(nBitpennyPoolExtraNonceBase);
    pblock->vtx[0].vin[0].scriptSig += COINBASE_FLAGS;

    assert(pblock->vtx[0].vin[0].scriptSig.size() <= 100);

    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}


Value getwork(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwork [data]\n"
            "If [data] is not specified, returns formatted hash data to work on:\n"
            "  \"midstate\" : precomputed hash state after hashing the first half of the data\n"
            "  \"data\" : block data\n"
            "  \"hash1\" : formatted hash buffer for second hash\n"
            "  \"target\" : little endian hash target\n"
            "If [data] is specified, tries to solve the block and returns true if it was successful.");

    if (vNodes.empty())
        throw JSONRPCError(-9, "BitPenny is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(-10, "BitPenny is downloading blocks...");

    static map<uint256, pair<WorkItem*, unsigned int> > mapNewBlock;
    static vector<WorkItem*> vNewBlock;
    static CReserveKey reservekey(pwalletMain);

    if (params.size() == 0)
    {
    	++nGetWorkCount;
        // Update block
        static unsigned int nTransactionsUpdatedLast;
        static CBlockIndex* pindexPrev;
        static int64 nStart;
        static WorkItem* pPoolWorkItem;
        static WorkItem* pSoloWorkItem;

		if (pindexPrev != pindexBest)
		{
			// Deallocate old blocks since they're obsolete now
			pPoolWorkItem = NULL;
			pSoloWorkItem = NULL;
			mapNewBlock.clear();
			BOOST_FOREACH(WorkItem* pwi, vNewBlock)
				delete pwi;
			vNewBlock.clear();
		}

		if (fBitpennyPoolMode)
		{
	        LOCK(cs_lBlockCandidates);
			bool fConnected = IsConnectedToBitpenny();

			if (nCurrentBlockCandidate > 0 && (!fConnected || nCurrentBlockCandidate < nBestHeight))
			{
				// outdated block candidates
				lBlockCandidates.clear();
				setBlockCandidates.clear();
				nCurrentBlockCandidate = 0;
				fNewPoolBlockIsAvailable = false;
				// can no longer use this
				pPoolWorkItem = NULL;
			}

			// check if we need a new pool block
			if (fConnected && nCurrentBlockCandidate >= nBestHeight && (pPoolWorkItem == NULL || fNewPoolBlockIsAvailable))
			{
				// do we have a suitable block?
				if (!lBlockCandidates.empty())
				{
					BlockCandidate* pbc = &lBlockCandidates.back();
					pPoolWorkItem = new WorkItem(pbc);
					if (!pPoolWorkItem)
						throw JSONRPCError(-7, "Out of memory");
					vNewBlock.push_back(pPoolWorkItem);
					fNewPoolBlockIsAvailable = false;
					// MinerLog("picking up new pool block\n");
				}
				else
					pPoolWorkItem = NULL;
			}
		}
		else
		{
			pPoolWorkItem = NULL;
		}

		if (pPoolWorkItem == NULL && fDisableSoloMode)
		{
			throw JSONRPCError(-9, "BitPenny solo mode disabled");
		}

		if (pPoolWorkItem == NULL && (pSoloWorkItem == NULL || (nTransactionsUpdated != nTransactionsUpdatedLast && (GetTime() - nStart) > 60)))
		{
			nTransactionsUpdatedLast = nTransactionsUpdated;
			nStart = GetTime();
			// Create new block
			CBlock* pblock = CreateNewBlock(reservekey);
			if (!pblock)
				throw JSONRPCError(-7, "Out of memory");

			// substitute an address
			// nothing should go to a local wallet. MerkleTree will be rebuilt by MinerIncrementExtraNonce
			pblock->vtx[0].vout[0].scriptPubKey = scriptMyPubKey;
			if (fPrintBlocks)
				pblock->print();

			pSoloWorkItem = new WorkItem(pblock);
			if (!pSoloWorkItem)
				throw JSONRPCError(-7, "Out of memory");
			vNewBlock.push_back(pSoloWorkItem);
		}

		pindexPrev = pindexBest;

		WorkItem* pwi;
		uint256 hashTarget;

		// check which one to use
		if (pPoolWorkItem)
		{
			pwi = pPoolWorkItem;
			hashTarget = hashBitpennyPoolTarget;

			if (fMiningSolo)
			{
				MinerLog("Switching back to pool mining!\n");
				fMiningSolo = false;
				nTimeMiningSolo +=  GetTimeMillis() - nTimeStartedMiningSolo;
				nTimeStartedMiningSolo = 0;
			}
		}
		else if (pSoloWorkItem)
		{
			pwi = pSoloWorkItem;
			hashTarget = CBigNum().SetCompact(pwi->pblock->nBits).getuint256();

			if (fMiningSolo == false)
			{
				fMiningSolo = true;
				MinerLog("Switching to solo mining!\n");
				nTimeStartedMiningSolo = GetTimeMillis();
			}
		}
		else
		{
			throw JSONRPCError(-7, "Out of memory");
		}

		CBlock* pblock = pwi->pblock;

		// Update nTime
		pblock->UpdateTime(pindexPrev);
		pblock->nNonce = 0;

		// Update nExtraNonce
		static unsigned int nExtraNonce = 0;
		MinerIncrementExtraNonce(pblock, nExtraNonce, pwi);

		// save
		mapNewBlock[pblock->hashMerkleRoot] = make_pair(pwi, nExtraNonce);

        // Prebuild hash buffers
		char pmidstate[32];
		char pdata[128];
		char phash1[64];

		FormatHashBuffers(pblock, pmidstate, pdata, phash1);

		Object result;
		result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate))));
		result.push_back(Pair("data",     HexStr(BEGIN(pdata), END(pdata))));
		result.push_back(Pair("hash1",    HexStr(BEGIN(phash1), END(phash1))));
		result.push_back(Pair("target",   HexStr(BEGIN(hashTarget), END(hashTarget))));
		return result;
    }
    else
    {
    	++ nHashCounter;
    	if (nHashTimerStart == 0)
    		nHashTimerStart = GetTimeMillis();

        if (GetTimeMillis() - nHashTimerStart > nStatsInterval)
        {
        	dPoolClientHashRate = 1000.0 * nHashCounter / (GetTimeMillis() - nHashTimerStart);
        	nHashTimerStart = GetTimeMillis();
        	nHashCounter = 0;
        }

    	// Parse parameters
        vector<unsigned char> vchData = ParseHex(params[0].get_str());
        if (vchData.size() != 128)
            throw JSONRPCError(-8, "Invalid parameter");
        CBlock* pdata = (CBlock*)&vchData[0];

        // Byte reverse
        for (int i = 0; i < 128/4; i++)
            ((unsigned int*)pdata)[i] = ByteReverse(((unsigned int*)pdata)[i]);

        // Get saved block
        WorkItem* pwi = NULL;
        unsigned int nExtraNonce;

        if (mapNewBlock.count(pdata->hashMerkleRoot))
        {
        	pwi = mapNewBlock[pdata->hashMerkleRoot].first;
        	nExtraNonce = mapNewBlock[pdata->hashMerkleRoot].second;
        }

        if (pwi != NULL && pwi->fPool)
        {
        	// check if work item is still alive
			LOCK(cs_lBlockCandidates);
			if (!setBlockCandidates.count(pwi->nBlockId))
				pwi = NULL;
        }

        if (pwi == NULL)
        {
        	MinerLog("ignore: hashMerkleRoot=%s: stale block\n", pdata->hashMerkleRoot.GetHex().c_str());
        	return false;
        }

        CBlock* pblock = pwi->pblock;

		pblock->nTime = pdata->nTime;
		pblock->nNonce = pdata->nNonce;
		pblock->vtx[0].vin[0].scriptSig = CScript() << pwi->nBlockId << CBigNum(nExtraNonce);
		if (pwi->fPool)
			pblock->vtx[0].vin[0].scriptSig << CBigNum(nBitpennyPoolExtraNonceBase);
		pblock->vtx[0].vin[0].scriptSig += COINBASE_FLAGS;

		pblock->hashMerkleRoot = pblock->BuildMerkleTree();

		uint256 hash = pblock->GetHash();

		if (hash > hashBitpennyPoolTarget)
		{
			MinerLog("ignore: hash=%s: t=%u: n=%u: e=%u: bad hash\n", hash.GetHex().c_str(), pblock->nTime, pblock->nNonce, nExtraNonce);
			return false;
		}

		uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

	    if (fDebug)	printf("MinerCheckWork: \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());

	    if (hash <= hashTarget)
	    {
			if (MinerCheckWork(hash, hashTarget, pblock, *pwalletMain))
			{
				MinerLog("Generated %s block %d\n", (pwi->fPool? "pool" : "solo"), nBestHeight);
			}
			else
			{
				// block was not accepted
				// do not submit it to the server to prevent forks, double-spending and other >50% network power attacks
				// uncomment line below only if pool gets close to 50%
				// return false;
			}
	    }

		bool rc = true;

		if (pwi->fPool)
		{
			++nHashesReceived;

			MinerLog("verify: id=%u: hash=%s: t=%u: n=%u: e=%u\n", pwi->nBlockId, hash.GetHex().c_str(), pblock->nTime, pblock->nNonce, nExtraNonce);

			if (IsConnectedToBitpenny())
			{
				pnodeBitpennyHost->PushMessage("bp:verify", pwi->nBlockId, hash, pblock->nTime, pblock->nNonce, nExtraNonce);
				++nSubmittedForVerification;
			}
			else
			{
				MinerLog("Connection to pool was lost\n");
				rc = false;
			}
		}

		return rc;
    }
}



////////////////////////////////////////
// main.cpp
////////////////////////////////////////

bool ProcessBitpennyMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
	if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        return false;
    }

    if (fDebug && strCommand.substr(0,3) == "bp:") {
        printf("%s ", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
        printf("received: %s (%d bytes)\n", strCommand.c_str(), vRecv.size());
    }

	if (strCommand == "bp:accepted")
	{
		uint256 hash;
		int64   ncredit;
		vRecv >> hash >> ncredit;
		++nHashesVerified;
		nSessionCredit += ncredit;
		MinerLog("accepted: hash=%s: credit=%s\n", hash.GetHex().c_str(), FormatMoney(ncredit).c_str());
	}
	else if (strCommand == "bp:pushwork")
	{
		BlockCandidate bc;

		vRecv >> bc.nBlockNumber >> bc.nBlockId >> bc.block >> nMinerBounty;

		LOCK(cs_lBlockCandidates);

		if (bc.nBlockNumber > nCurrentBlockCandidate)
		{
			lBlockCandidates.clear();
			setBlockCandidates.clear();
			nCurrentBlockCandidate = 0;
			fNewPoolBlockIsAvailable = false;
		}

		if (bc.nBlockNumber >= nBestHeight)
		{
			lBlockCandidates.push_back(bc);
			setBlockCandidates.insert(bc.nBlockId);
			nCurrentBlockCandidate = bc.nBlockNumber;
			fNewPoolBlockIsAvailable = true;

			int64 nMyValueOut = 0;
			int64 nBlockValueOut = 0;
			BOOST_FOREACH(const CTxOut& txout, bc.block.vtx[0].vout)
			{
				nBlockValueOut += txout.nValue;
				if (txout.scriptPubKey == scriptMyPubKey)
					nMyValueOut = txout.nValue;
			}

			nMyShareInCurrentBlock = nMyValueOut;

			MinerLog("block candidate %d id=%u with %d transactions. Coinbase value %s. My share is %s BTC.\n",
					bc.nBlockNumber, bc.nBlockId, bc.block.vtx.size(), FormatMoney(nBlockValueOut, false).c_str(), FormatMoney(nMyValueOut, false).c_str());

			if (fPrintBlocks)
				bc.block.print();

			// we are mining solo. let miner know that pool block is available
			// helpful for miners using nTime rotation
			if (fBlockMonitor && fMiningSolo)
					BlockMonitor("Pool Block");
		}
		else
			printf("Stale block %d from Bitpenny\n", bc.nBlockNumber);
	}
	else if (strCommand == "bp:rejected")
	{
		uint256 hash;
		string smsg;
		vRecv >> hash >> smsg;
		++nHashesRejected;
		MinerLog("rejected: hash=%s: msg=%s\n", hash.GetHex().c_str(), smsg.c_str());
	}
	else if (strCommand == "bp:welcome")
	{
		vRecv >> strBitpennyPoolName >> nBitpennyPoolDifficulty >> dBitpennyPoolFeePerCent >> hashBitpennyPoolTarget >> nBitpennyPoolExtraNonceBase;
		MinerLog("welcome: pool=%s difficulty=%d fee=%f target=%s extranoncebase=%u\n", strBitpennyPoolName.c_str(), nBitpennyPoolDifficulty, dBitpennyPoolFeePerCent, hashBitpennyPoolTarget.GetHex().c_str(), nBitpennyPoolExtraNonceBase);
	}
	else if (strCommand == "bp:s_stats")
	{
		LOCK(cs_ServerStats);
		vRecv >> ServerStats;
		MinerLog("s_stats: %s\n", ServerStats.ToStr().c_str());
	}
	else if (strCommand == "bp:c_stats")
	{
		LOCK(cs_ServerStats);
		vRecv >> ClientStats;
		MinerLog("c_stats: %s\n", ClientStats.ToStr().c_str());
	}
	else if (strCommand == "bp:message")
	{
		vRecv >> strBitpennyPoolMessage;
		MinerLog("message: %s\n", strBitpennyPoolName.c_str(), strBitpennyPoolMessage.c_str());
	}
	else if (strCommand == "bp:badclient")
	{
		vRecv >> strBitpennyPoolName >> strBitpennyPoolMessage;
		MinerLog("badclient: pool=%s message=%s\n", strBitpennyPoolName.c_str(), strBitpennyPoolMessage.c_str());
		UsePool(false);
	}
	else
	{
		return false;
	}

	return true;
}





// Block Monitor
void BlockMonitor(const char *msg)
{
	SOCKET hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (hSocket == INVALID_SOCKET)
	{
		printf("BlockMonitor: notification failed. Could not create the socket %i\n", WSAGetLastError());
	}
	else
	{
		char buf[32];

		if (msg == NULL)
		{
			sprintf(buf, "New Block %d", nBestHeight);
			msg = buf;
		}

		int buflen = strlen(msg);

		struct sockaddr_in saddr;
		int addrlen = sizeof(saddr);

		{
			LOCK(cs_sBlockMonitorTargets);
			BOOST_FOREACH(CService target, sBlockMonitorTargets)
			{
				if(target.GetSockAddr(&saddr))
				{
					// this is done on a best effort basis. hosts might come and go
					sendto(hSocket, msg, buflen, 0, (struct sockaddr *)&saddr, addrlen);
				}
			}
		}

		closesocket(hSocket);
	}
}

// load block monitor targets from config file
// -blockmonitortarget is in host:port format
static void LoadBlockMonitorTargets()
{

	LOCK(cs_sBlockMonitorTargets);

	sBlockMonitorTargets.clear();

	BOOST_FOREACH(string line, mapMultiArgs["-blockmonitortarget"])
	{
		size_t nPos = line.rfind(':');
		if (nPos == string::npos || nPos == 0)
		{
			printf("blockmonitortarget invalid address: %s. Parameter format is host:port.\n", line.c_str());
			continue;
		}

		string host, port;
		host.assign(line.begin(), line.begin() + nPos);
		port.assign(line.begin() + nPos + 1, line.end());

		if (fDebug)
			printf("blockmonitortarget host=%s port=%s\n", host.c_str(), port.c_str());

		int nPort = lexical_cast<int>(port);

		CService addr(host, nPort, fAllowDNS);

		if (addr.IsValid())
		{
			sBlockMonitorTargets.insert(addr);
		}
		else
		{
			printf("blockmonitortarget invalid address: %s\n", line.c_str());
			continue;
		}
	}
}
