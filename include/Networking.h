/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Common networking routines to help us
// Created 18/12/02
// Jason Boettcher


#ifndef __NETWORKING_H__
#define __NETWORKING_H__

#include <string>
#include <SDL.h>
#include "types.h"
#include "Utils.h"

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif


#define		LX_SVRLIST		"/svr_list.php"
#define		LX_SVRREG		"/svr_register.php"
#define		LX_SVRDEREG		"/svr_deregister.php"


/* Shplorbs host, using freesql
#define		LX_SITE			"shplorb.ods.org"
#define		LX_SVRLIST		"/~jackal/svr_list.php"
#define		LX_SVRREG		"/~jackal/svr_register.php"
#define		LX_SVRDEREG		"/~jackal/svr_deregister.php"
*/


/* Old host
#define		LX_SITE			"host.deluxnetwork.com"
#define		LX_SVRLIST		"/~lierox/gamefiles/svr_list.php"
#define		LX_SVRREG		"/~lierox/gamefiles/svr_register.php"
#define		LX_SVRDEREG		"/~lierox/gamefiles/svr_deregister.php"
*/

#define		LX_SVTIMEOUT	35
#define		LX_CLTIMEOUT	30
#define     DNS_TIMEOUT		10

// socket address; this type will be given around as pointer
DEFINE_INTERNDATA_CLASS(NetworkAddr);

// socket itself; the structure/type itself will be given around
DEFINE_INTERNDATA_CLASS(NetworkSocket);

// general networking
bool	InitNetworkSystem();
bool	QuitNetworkSystem();
NetworkSocket	OpenReliableSocket(unsigned short port);
NetworkSocket	OpenUnreliableSocket(unsigned short port, bool events = true);
NetworkSocket	OpenBroadcastSocket(unsigned short port, bool events = true);
bool	ConnectSocket(NetworkSocket sock, const NetworkAddr& addr);
bool	ListenSocket(NetworkSocket sock);
bool	CloseSocket(NetworkSocket& sock);
int		WriteSocket(NetworkSocket sock, const void* buffer, int nbytes);
int		WriteSocket(NetworkSocket sock, const std::string& buffer);
int		ReadSocket(NetworkSocket sock, void* buffer, int nbytes);
bool	IsSocketStateValid(NetworkSocket sock);
bool	IsSocketReady(NetworkSocket sock);
void	InvalidateSocketState(NetworkSocket& sock);
void AddSocketToNotifierGroup( NetworkSocket sock );
void RemoveSocketFromNotifierGroup( NetworkSocket sock );
void	WaitForSocketWrite(NetworkSocket sock, int timeout);
void	WaitForSocketRead(NetworkSocket sock, int timeout);
void	WaitForSocketReadOrWrite(NetworkSocket sock, int timeout);

int		GetSocketErrorNr();
std::string	GetSocketErrorStr(int errnr);
std::string	GetLastErrorStr();
bool	IsMessageEndSocketErrorNr(int errnr);
void	ResetSocketError();

bool	GetLocalNetAddr(NetworkSocket sock, NetworkAddr& addr);
bool	GetRemoteNetAddr(NetworkSocket sock, NetworkAddr& addr);
bool	SetRemoteNetAddr(NetworkSocket sock, const NetworkAddr& addr);
bool	IsNetAddrValid(const NetworkAddr& addr);
bool	SetNetAddrValid(NetworkAddr& addr, bool valid);
void	ResetNetAddr(NetworkAddr& addr);
bool	StringToNetAddr(const std::string& string, NetworkAddr& addr);
bool	NetAddrToString(const NetworkAddr& addr, std::string& string);
unsigned short GetNetAddrPort(const NetworkAddr& addr);
bool	SetNetAddrPort(NetworkAddr& addr, unsigned short port);
bool	AreNetAddrEqual(const NetworkAddr& addr1, const NetworkAddr& addr2);
bool	GetNetAddrFromNameAsync(const std::string& name, NetworkAddr& addr);
void	AddToDnsCache(const std::string& name, const NetworkAddr& addr, TimeDiff expireTime = TimeDiff(3600.0f));
bool	GetFromDnsCache(const std::string& name, NetworkAddr& addr);
bool	isDataAvailable(NetworkSocket sock); // Slow!

#endif  //  __NETWORKING_H__
