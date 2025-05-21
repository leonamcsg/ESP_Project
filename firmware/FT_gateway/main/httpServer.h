/**
 * @file httpServer.h
 * @brief 
 * @details
 * @author Luiz Carlos
 * @date 2024-11-17
 */

#ifndef MAIN_HTTPSERVER_H_
#define MAIN_HTTPSERVER_H_


/**************************
**		  INCLUDES	 	 **
**************************/

// ESP libraries
#include "esp_http_server.h"

/**************************
**		DEFINITIONS		 **
**************************/

#define URI_FUNCTION_HANDLER_NAME(uri_handler) http_server_ ## uri_handler ## _handler
#define BINARY_START(bin,uri_handler,s) #bin###uri_handler###s

#define HTTP_SERVER_MAX_URI_HANDLERS	20

/**
 * @brief Server timeout in seconds
 */
#define HTTP_SERVER_TIMEOUT_LIMIT		10 //seconds

/**
 * @brief Creating Server State list with X_MACRO 
 * 
 */
#define X_MACRO_HTTP_SERVER_STATE_LIST	\
	X(0, HTTP_WIFI_CONNECT_INIT			) \
	X(1, HTTP_WIFI_CONNECT_SUCCESS		) \
	X(2, HTTP_WIFI_CONNECT_FAIL			) \
	X(3, HTTP_OTA_UPDATE_SUCCESSFULL	) \
	X(4, HTTP_OTA_UPDATE_FAILED			)

/**
 * @brief Creating Routes with X_MACRO
 * 
 * Explanation of how to add more routes using X_MACRO
 */
#define X_MACRO_HTTP_SERVER_URI_HANDLER_LIST\
	X(jquery_3_3_1_min_js,	"/jquery-3.3.1.min.js",	"application/javascript", _binary_jquery_3_3_1_min_js_start,	_binary_jquery_3_3_1_min_js_end	) \
	X(index_html,			"/index.html",			"text/html"				, _binary_index_html_start, 			_binary_index_html_end			) \
	X(app_css,				"/app.css",				"text/css"				, _binary_app_css_start, 				_binary_app_css_end				) \
	X(app_js,				"/app.js",				"application/javascript", _binary_app_js_start, 				_binary_app_js_end				) \
	X(favicon_ico,			"/favicon_ico",			"image/x-icon"			, _binary_favicon_ico_start, 			_binary_favicon_ico_end			)


/**************************
**		STRUCTURES		 **
**************************/

/**
 * Connection status for WiFi
 */
typedef enum http_server_wifi_connect_status
{
	NONE = 0,
	HTTP_WIFI_STATUS_CONNECTING,
	HTTP_WIFI_STATUS_CONNECT_FAILED,
	HTTP_WIFI_STATUS_CONNECT_SUCCESS,
} http_server_wifi_connect_status_e;

/**
 * Messages for the HTTP monitor
 */
typedef enum http_server_state
{
	#define X(ID, ENUM) ENUM=ID, 
		X_MACRO_HTTP_SERVER_STATE_LIST
	#undef X
} http_server_state_e;

/**
 * Structure for the message queue
 */
typedef struct http_server_queue_message_s
{
	http_server_state_e msgId;
} http_server_queue_message_t;

/**************************
**		FUNCTIONS		 **
**************************/

/**
 * @brief Sends a message to the queue
 * @param msgId message ID from the http_server_state_e enum.
 * @return pdTRUE if an item was successfully sent to the queue, otherwise pdFALSE.
 */
BaseType_t httpServer_monitor_sendMessage(http_server_state_e msgId);
 
/**
 * Starts the HTTP server.
 */
void httpServer_start(void);

/**
 * Stops the HTTP server.
 */
void httpServer_stop(void);


/**************************
** UPPERLAYER FUNCTIONS	 **
**************************/

/**
 * @brief Returns the g_wifi_connect_status
 * 
 * @return http_server_wifi_connect_status_e * 
 */
http_server_wifi_connect_status_e * httpServer_get_wifiConnectStatus(void);

/**
 * Function to get routers from another file to be declared here.
 * Separating the include files routes, from the api ones.
 * 
 * @param apiFunction a function from an upper layer, where other
 * uri routes are declared with the httpServer_uri_registerHandler function.
 */
void httpServer_setApiRoutes(void (*function)(void));

/**
 * Function to register an HTTP uri.
 * 
 * @param route the http route on the IP server
 * @param method if the route is accessed by GET, POST, PUT...
 * @param handler the function handler that specifies what happens when the route is accessed.
 */
void httpServer_uri_registerHandler(const char* route, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *req));

#endif /* MAIN_HTTPSERVER_H_ */
