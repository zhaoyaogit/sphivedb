/*
 * Copyright 2009 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "sphivedbcli.hpp"
#include "sphivecomm.hpp"
#include "sphivemsg.hpp"

#include "spnetkit/spnklist.hpp"
#include "spnetkit/spnktime.hpp"
#include "spnetkit/spnklog.hpp"
#include "spnetkit/spnksocket.hpp"

static int gClients = 8;
static int gReadTimes = 100;
static int gWriteTimes = 100;
static const char * gConfigFile = "sphivedbcli.ini";
static int gTableKeyMax = 1000;

static pthread_mutex_t gBeginMutex = PTHREAD_MUTEX_INITIALIZER;

void showUsage( const char * program )
{
	printf( "\n" );
	printf( "Usage: %s [-f <config file>] [-c <clients>] [-r <read times>] [-w <write times>] [-o] [-v]\n", program );
	printf( "\t-f specify sphivedbcli.ini\n" );
	printf( "\t-c how many clients\n" );
	printf( "\t-r how many read actions per client\n" );
	printf( "\t-w how many write actions per client\n" );
	printf( "\t-o log socket IO\n" );
	printf( "\t-v show this usage\n" );
	printf( "\n" );

	exit ( 0 );
}

void * threadFunc( void * args )
{
	// waiting for start mutex
	pthread_mutex_lock( &gBeginMutex );
	pthread_mutex_unlock( &gBeginMutex );

	SP_HiveDBClient * client = (SP_HiveDBClient*)args;

	SP_NKStringList readSql, writeSql;
	readSql.append( "select * from addrbook limit 100;" );

	int loops = gReadTimes + gWriteTimes;

	int maxUid = gTableKeyMax * 100;

	SP_NKClock clock;
	int writeTimes = 0;

	for( int i = 0; i < loops; i++ ) {
		int isWrite = ( random() % loops ) > gReadTimes;
		int uid = random() % maxUid;

		char user[ 32 ] = { 0 };
		snprintf( user, sizeof( user ), "%d", uid );

		SP_NKStringList * actionSql = &readSql;

		if( isWrite ) {
			char sql[ 256 ] = { 0 };
			snprintf( sql, sizeof( sql ), "insert into addrbook ( gid, addr, freq ) "
					"values ( 0, '%d.%d', 0 );", i, (int)time( NULL ) );

			writeSql.clean();
			writeSql.append( sql );

			actionSql = &writeSql;

			writeTimes++;
		}

		SP_HiveRespObject * resp = client->execute( uid / 100, user, "addrbook", actionSql );
		if( NULL != resp ) {
			delete resp;
		} else {
			if( 0 == ( i % 100 ) ) printf( "socket fail\n" );
		}
	}

	int usedTime = clock.getAge();
	int readTimes = loops - writeTimes;

	float writePerSeconds = ( writeTimes * 1000.0 ) / usedTime;
	float readPerSeconds = ( readTimes * 1000.0 ) / usedTime;

	printf( "Used Time: %d (ms), Write %d (%.2f), Read %d (%.2f)\n",
			usedTime, writeTimes, writePerSeconds, readTimes, readPerSeconds );

	return NULL;
}

int main( int argc, char * argv[] )
{
	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "f:c:r:w:ov" ) ) != EOF ) {
		switch ( c ) {
			case 'f':
				gConfigFile = optarg;
				break;
			case 'c':
				gClients = atoi( optarg );
				break;
			case 'r':
				gReadTimes = atoi( optarg );
				break;
			case 'w':
				gWriteTimes = atoi( optarg );
				break;
			case 'o':
				SP_NKLog::setLogLevel( LOG_DEBUG );
				SP_NKLog::init4test( "teststress" );
				SP_NKSocket::setLogSocketDefault( 1 );
				break;
			case '?' :
			case 'v' :
				showUsage( argv[0] );
		}
	}

	SP_HiveDBClient client;

	assert( 0 == client.init( gConfigFile ) );

	pthread_t * thrlist = (pthread_t*)calloc( sizeof( pthread_t ), gClients );

	pthread_mutex_lock( &gBeginMutex );

	printf( "begin to create thread ...\n" );

	for( int i = 0; i < gClients; i++ ) {
		if( 0 != pthread_create( &(thrlist[i]), NULL, threadFunc, &client ) ) {
			printf( "pthread_create fail, index %d, errno %d, %s\n", i, errno, strerror( errno ) );
			exit( -1 );
		}
	}

	printf( "begin to execute ...\n" );

	pthread_mutex_unlock( &gBeginMutex );

	for( int i = 0; i < gClients; i++ ) {
		pthread_join( thrlist[i], NULL );
	}

	return 0;
}
