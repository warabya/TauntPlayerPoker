/*
 * TeamSpeak 3 Taunt Player Poker plugin
 *
 */
#define PLUGIN_API_VERSION 20
#define MAX_LENGTH_OF_TAUNT_CODE 32

#ifdef WINDOWS
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "public_errors.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include <math.h>
#include "curl.h"


static struct TS3Functions ts3Functions;

#ifdef WINDOWS
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); dest[destSize-1] = '\0'; }
#endif

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128

static char* pluginCommandID = NULL;
/* Define and init the random number class instance.  No return values, so assume it works :( */
//CRandomMersenne RanGen((int)time(0));

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
    return "TauntPlayerPoker";
}

/* Plugin version */
const char* ts3plugin_version() {
    return "0.1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
        return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
    return "wara_be";
}

/* Plugin description */
const char* ts3plugin_description() {
    return "This plugin poke an external program for playing a taunt sounds.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {

    /* Your plugin init code here:
           We don't have any init code, so lets just do a test to the console to make sure it works. */
        //printf("RandomNumber Test: %d\n", RanGen.IRandom(0,99));

    return 0;  /* 0 = success, 1 = failure */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */

        /* Free pluginCommandID if we registered it */
        if(pluginCommandID) {
                free(pluginCommandID);
                pluginCommandID = NULL;
        }
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
        printf("PLUGIN: offersConfigure\n");
        /*
         * Return values:
         * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
         * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
         * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
         */
        return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window.If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    printf("PLUGIN: configure\n");
}

/*
* If the plugin wants to use plugin commands, it needs to register a command ID. This function will be automatically called after
* the plugin was initialized. This function is optional. If you don't use plugin commands, the function can be omitted.
* Note the passed commandID parameter is no longer valid after calling this function, so you *must* copy it and store it in the plugin.
*/
//void ts3plugin_registerPluginCommandID(const char* commandID) {
//      const size_t sz = strlen(commandID) + 1;
//      pluginCommandID = (char*)malloc(sz);
//      memset(pluginCommandID, 0, sz);
//      _strcpy(pluginCommandID, sz, commandID);  /* The commandID buffer will invalidate after existing this function */
//      printf("PLUGIN: registerPluginCommandID: %s\n", pluginCommandID);
//}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
        return "";
}

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {

    if(newStatus == STATUS_CONNECTION_ESTABLISHED) {  /* connection established and we have client and channels available */
        anyID myID;

        /* Get client's ID */
        if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
                else
                {
                        //Bring up a PM tab for yourself
                        if(ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "This is your private dice rolling area.", myID, NULL) != ERROR_ok) {
                                ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                        }
                }
    }
}

int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
    printf("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);

	// -tから始まるテキストチャットを判別
	if(message[0] == '-' && message[1] == 't') {
	    // テキストチャットからtauntIdentifierを取得
		int i;
		char* tauntIdentifier;
		tauntIdentifier = new char[MAX_LENGTH_OF_TAUNT_CODE];

		for (i=0; message[i+2]; i++) {
			tauntIdentifier[i] = message[i+2];
		}
		tauntIdentifier[i+1] = NULL;
 
		// こっから、curlでhttpリクエスト発行する
		CURL *curl;
		CURLcode res;
		// TODO: global_initとかついてっけどこのstatement何度も呼ばれるんよね。大丈夫？ts3plugin_init()あたりに移動したほうがええんかな？
		curl_global_init(CURL_GLOBAL_ALL);
		curl = curl_easy_init();
		if(curl) {
			char URL[256] = "http://localhost:2222/?tauntIdentifier=";
		    strcat_s(URL, 256, tauntIdentifier);
			curl_easy_setopt(curl, CURLOPT_URL,URL);
 
			// HTTPリクエストを発行、resを取得
			res = curl_easy_perform(curl);

			// 成功の場合、各テキストチャット発言先に再生するtauntのidentifierを表示
			// TODO: 接頭辞つけて「taunt play request(id: 100)」とかのがそれっぽいよね、そのうちそうしたいね。
			if(res == CURLE_OK) {
				switch (targetMode)     {
				case TextMessageTarget_CLIENT:
						anyID myID;
						if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
								ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
								return 0;
						}
						if(ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, tauntIdentifier, fromID, NULL) != ERROR_ok) {
								ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
						}
						break;

				case TextMessageTarget_CHANNEL:
						if(ts3Functions.requestSendChannelTextMsg(serverConnectionHandlerID, tauntIdentifier, fromID, NULL) != ERROR_ok) 
						{
								ts3Functions.logMessage("Error requesting send text message to channel", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
						}
						break;

				case TextMessageTarget_SERVER:
						if(ts3Functions.requestSendServerTextMsg(serverConnectionHandlerID, tauntIdentifier, NULL) != ERROR_ok) 
						{
								ts3Functions.logMessage("Error requesting send text message to channel", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
						}
						break;
				}
			}

			// エラーチェック
			if(res != CURLE_OK)
				// TODO: エラーのときどうしようね？
				fprintf(stderr, "curl_easy_perform() failed: %s\n",
						curl_easy_strerror(res));
 
			// 後処理
			curl_easy_cleanup(curl);
		}
		curl_global_cleanup();

	}

	return 0;
}