/**
 * @file router.c
 * @brief 
 * @details
 * @date 17 de nov. de 2024
 * @author Luiz Carlos
 */


/**************************
**		  INCLUDES	 	 **
**************************/

// C libraries
#include <stdio.h>
#include <string.h>

// ESP libraries
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "esp_ota_ops.h"
#include "sys/param.h"

// Personal libraries
#include "httpServer.h"
#include "router.h"



/**************************
**		DECLARATIONS	 **
**************************/

	/* Variables */

// Tag used for ESP serial console messages
static const char TAG[] = "http_app_server";

// Netif object for the Station and Access Point
extern esp_netif_t * esp_netif_sta;
extern esp_netif_t * esp_netif_ap;

// Buffer for LocalTime json string
char localJSONObjBuffer[BUFFER_MAX_SIZE] = {0};
char localTimeJSON[BUFFER_MAX_SIZE] = {0};


/* Static Functions */

// URI handler functions
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(wifi_connect_json)(httpd_req_t *req);
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(wifi_connect_status_json)(httpd_req_t *req);
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(get_wifi_connect_info_json)(httpd_req_t *req);
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(wifi_disconnect_json)(httpd_req_t *req);
esp_err_t APP_URI_FUNCTION_HANDLER_NAME(http_server_OTA_update_handler)(httpd_req_t *req);
esp_err_t APP_URI_FUNCTION_HANDLER_NAME(http_server_OTA_status_handler)(httpd_req_t *req);
static void router_uri_register(void);



/**************************
**	   APP FUNCTIONS 	 **
**************************/

void router_setup(void)
{
	// Allocate the api routes inside the httpServer code
	httpServer_setApiRoutes(&router_uri_register);
	
	// Start WiFi
	wifiApp_start();
	
	// Clean localTimeJSON buffer
	memset(localTimeJSON, 0, BUFFER_MAX_SIZE);
}



/**************************
**	 HANDLER FUNCTIONS 	 **
**************************/

/**
 * Receives the .bin file fia the web page and handles the firmware update
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started.
 */
esp_err_t APP_URI_FUNCTION_HANDLER_NAME(http_server_OTA_update_handler)(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	bool flash_successful = false;

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	do
	{
		// Read the data for the request
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
		{
			// Check if timeout occurred
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
				continue; ///> Retry receiving if timeout occurred
			}
			ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
			return ESP_FAIL;
		}
		printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

		// Is this the first data we are receiving
		// If so, it will have the information in the header that we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;

			// Get the location of the .bin file content (remove the web form data)
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{
				printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, update_partition->address);
			}

			// Write this first part of the data
			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += body_part_len;
		}
		else
		{
			// Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}

	} while (recv_len > 0 && content_received < content_length);

	if (esp_ota_end(ota_handle) == ESP_OK)
	{
		// Lets update the partition
		if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
		{
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
			ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
			flash_successful = true;
		}
		else
		{
			ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
		}
	}
	else
	{
		ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
	}

	// We won't update the global variables throughout the file, so send the message about the status
	if (flash_successful) { httpServer_monitor_sendMessage(HTTP_OTA_UPDATE_SUCCESSFULL); } else { httpServer_monitor_sendMessage(HTTP_OTA_UPDATE_FAILED); }

	return ESP_OK;
}

/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * 
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
esp_err_t APP_URI_FUNCTION_HANDLER_NAME(http_server_OTA_status_handler)(httpd_req_t *req)
{
	char otaJSON[100];
	ESP_LOGI(TAG, "OTAstatus requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));

	return ESP_OK;
}

/**
 * wifiConnect.json handler is invoked after the connect button is pressed
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(wifi_connect_json)(httpd_req_t *req)
{	
	// char localJSONObjBuffer[BUFFER_MAX_SIZE];
	size_t lenBodyJson = 0;
	size_t lenPass;
	size_t lenSSID;

	char * ssid_p;
	char * pwd_p;
	wifi_config_t * wifi_config_p = NULL;
	
	ESP_LOGI(TAG, "/wifiConnect.json requested");
	
	memset(localJSONObjBuffer,0, BUFFER_MAX_SIZE);

	// Get Request Body
	lenBodyJson = req->content_len;
	httpd_req_recv(req, localJSONObjBuffer, lenBodyJson);
	printf("Data sent by the client: %.*s\n",lenBodyJson, localJSONObjBuffer);

	// Parse the JSON data (example using cJSON)
	cJSON *body_json = cJSON_Parse(localJSONObjBuffer);
	if (!body_json) {
		ESP_LOGE(TAG, "Failed to parse JSON data");
		cJSON_Delete(body_json);
		return ESP_FAIL;
	}

	cJSON *ssid_json = cJSON_GetObjectItemCaseSensitive(body_json, "c_ssid");
	cJSON *pwd_json = cJSON_GetObjectItemCaseSensitive(body_json, "c_pwd");

	if (!ssid_json || !pwd_json) {
		ESP_LOGE(TAG, "Missing 'c_ssid' or 'c_pwd' in JSON data");
		cJSON_Delete(body_json);
		return ESP_FAIL;
	}

	if (!cJSON_IsString(ssid_json) || !cJSON_IsString(pwd_json)) {
		ESP_LOGE(TAG, "Invalid data type for 'c_ssid' or 'c_pwd'");
		cJSON_Delete(body_json);
		return ESP_FAIL;
	}


	// Get SSID from body
	ssid_p = wifiApp_getStationSSID();
	memset(ssid_p, 0x00, WIFI_SSID_LENGTH);
	ssid_p = ssid_json->valuestring;
	lenSSID = strlen(ssid_p);
	
	// Get Password from body
	pwd_p = wifiApp_getStationPassword();
	memset(pwd_p, 0x00, WIFI_PASSWORD_LENGTH);
	pwd_p = pwd_json->valuestring;
	lenPass = strlen(pwd_p);
	
	// Update the WiFi networks configuration and let the WiFi applications know
	wifi_config_p = wifiApp_getWifiConfig();
	memset(wifi_config_p, 0x00, sizeof(wifi_config_t));
	memcpy(wifi_config_p->sta.ssid, ssid_p, lenSSID);
	memcpy(wifi_config_p->sta.password, pwd_p, lenPass);
	wifiApp_sendMessage(WIFI_APP_CONNECTING_FROM_HTTP_SERVER);

	cJSON_Delete(body_json);
	
	return ESP_OK;
}



/**
 * wifiConnectStatus handler updates the connection status for the web page.
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(wifi_connect_status_json)(httpd_req_t *req)
{	
	ESP_LOGI(TAG, "/wifiConnectStatus requested");
	
	memset(localJSONObjBuffer, 0, BUFFER_MAX_SIZE);
	
	sprintf(localJSONObjBuffer, "{\"wifi_connect_status_json\":%d}", *httpServer_get_wifiConnectStatus());
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, localJSONObjBuffer, strlen(localJSONObjBuffer));
	
	return ESP_OK;
}

/**
 * wifiConnectInfo.json handler updates the web page with connection information.
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(get_wifi_connect_info_json)(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectInfo.json requested");
	
	char ipInfoJSON[200];
	memset(ipInfoJSON, 0x00, sizeof(ipInfoJSON));
	
	char ip[IP4ADDR_STRLEN_MAX];
	char netmask[IP4ADDR_STRLEN_MAX];
	char gateway[IP4ADDR_STRLEN_MAX];

	if (*httpServer_get_wifiConnectStatus() == HTTP_WIFI_STATUS_CONNECT_SUCCESS)
	{
		wifi_ap_record_t wifi_data;
		ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));
		char * ssid = (char*)wifi_data.ssid;
		
		esp_netif_ip_info_t ip_info;
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));
		esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.gw, gateway, IP4ADDR_STRLEN_MAX);
		
		sprintf(ipInfoJSON, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gateway\":\"%s\",\"ap\":\"%s\"}", ip, netmask, gateway, ssid);
	}
	
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));
	
	return ESP_OK;
}

/**
 * wifiDisconnect.json handler responds by sending a message to WiFi application to disconnect.
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t APP_URI_FUNCTION_HANDLER_NAME(wifi_disconnect_json)(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiDisconnect.json requested");
	
	wifiApp_sendMessage(WIFI_APP_USER_REQUESTED_STA_DISCONNECT);
	
	return ESP_OK;
}

/**
 * A function that will make the uri's available to the server.
 */
static void router_uri_register(void)
{
	#define X(id, handler, route, method, ansType) \
		httpServer_uri_registerHandler(route, method, APP_URI_FUNCTION_HANDLER_NAME(handler));
		X_MACRO_API_ROUTES_LIST
	#undef X
}